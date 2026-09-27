// Exact search over the existing policies and all positive ordered capacity splits.
#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache.h"

using Requests_t = std::vector<int>;

namespace {

const std::array<std::string, 5> policyNames = {"LRU", "LFU", "ARC", "2Q", "LIRS"};

struct Trace
{
    Requests_t requests;
    std::size_t repetitionCount = 1;
};

using Traces_t = std::vector<Trace>;

struct Configuration
{
    std::vector<std::size_t> capacities;
    std::vector<int> policyIndices;
    std::size_t misses = std::numeric_limits<std::size_t>::max();
};

bool isPolicySupported(int policyIndex, std::size_t capacity)
{
    return capacity >= ((policyIndex == 3 || policyIndex == 4) ? 2u : 1u);
}

std::unique_ptr<cache::Cache<int, int>> makeCache(int policyIndex, std::size_t capacity)
{
    switch (policyIndex)
    {
        case 0:
            return std::make_unique<cache::CacheLRU<int, int>>(capacity);

        case 1:
            return std::make_unique<cache::CacheLFU<int, int>>(capacity);

        case 2:
            return std::make_unique<cache::CacheARC<int, int>>(capacity);

        case 3:
            return std::make_unique<cache::Cache2Q<int, int>>(capacity);

        case 4:
            return std::make_unique<cache::CacheLIRS<int, int>>(capacity);

        default:
            throw std::invalid_argument("Unknown policy");
    }
}

Requests_t collectMissRequests(const Requests_t& requests, int policyIndex, std::size_t capacity)
{
    auto instance = makeCache(policyIndex, capacity);
    Requests_t misses;
    misses.reserve(requests.size());

    for (int key : requests)
    {
        instance->lookupUpdate(key, [&misses](int requestedKey)
        {
            misses.push_back(requestedKey);
            return requestedKey;
        });
    }

    return misses;
}

std::size_t countMisses(const Requests_t& requests, int policyIndex, std::size_t capacity,
                        std::size_t missLimit)
{
    auto instance = makeCache(policyIndex, capacity);
    std::size_t misses = 0;

    for (int key : requests)
    {
        instance->lookupUpdate(key, [&misses](int requestedKey)
        {
            ++misses;
            return requestedKey;
        });

        if (misses >= missLimit)
        {
            break; // A non-improving candidate cannot recover later.
        }
    }

    return misses;
}

std::size_t countIdealMisses(const Requests_t& requests, std::size_t capacity)
{
    std::unordered_map<int, std::size_t> nextPositions;
    std::vector<std::size_t> nextUse(requests.size(), requests.size());

    for (std::size_t index = requests.size(); index-- > 0;)
    {
        auto found = nextPositions.find(requests[index]);

        if (found != nextPositions.end())
        {
            nextUse[index] = found->second;
        }

        nextPositions[requests[index]] = index;
    }

    using HeapEntry_t = std::pair<std::size_t, int>;
    std::priority_queue<HeapEntry_t> evictionQueue;
    std::unordered_map<int, std::size_t> residentNextUse;
    std::size_t misses = 0;

    for (std::size_t index = 0; index < requests.size(); ++index)
    {
        const int key = requests[index];

        if (residentNextUse.find(key) == residentNextUse.end())
        {
            ++misses;

            if (residentNextUse.size() == capacity)
            {
                while (true)
                {
                    const auto candidate = evictionQueue.top();
                    evictionQueue.pop();

                    auto found = residentNextUse.find(candidate.second);

                    if (found != residentNextUse.end() && found->second == candidate.first)
                    {
                        residentNextUse.erase(found);
                        break;
                    }
                }
            }
        }

        residentNextUse[key] = nextUse[index];
        evictionQueue.push({nextUse[index], key});

        if (evictionQueue.size() > 4 * capacity)
        {
            evictionQueue = {};

            for (const auto& entry : residentNextUse)
            {
                evictionQueue.push({entry.second, entry.first});
            }
        }
    }

    return misses;
}

class ExactSearch
{
private:
    const Traces_t& inputTraces_;
    std::size_t totalCapacity_;
    int maxLevels_;
    std::size_t theoreticalMinimum_;

    Configuration best_;

    std::size_t evaluatedCandidates_ = 0;
    std::size_t prunedBranches_ = 0;

    std::size_t lowerBound(const Traces_t& traces, std::size_t capacity) const
    {
        std::size_t result = 0;

        for (const auto& trace : traces)
        {
            result += trace.repetitionCount * countIdealMisses(trace.requests, capacity);
        }

        return result;
    }

    void evaluateLastLevel(const Traces_t& traces, std::size_t capacity,
                           std::vector<std::size_t>& selectedCapacities,
                           std::vector<int>& policyIndices)
    {
        for (int policyIndex = 0; policyIndex < static_cast<int>(policyNames.size()); ++policyIndex)
        {
            if (!isPolicySupported(policyIndex, capacity))
            {
                continue;
            }

            ++evaluatedCandidates_;
            std::size_t totalMisses = 0;

            for (const auto& trace : traces)
            {
                const auto remaining = best_.misses - totalMisses;
                const auto missLimit =
                    remaining / trace.repetitionCount + (remaining % trace.repetitionCount != 0);

                totalMisses += trace.repetitionCount *
                               countMisses(trace.requests, policyIndex, capacity, missLimit);

                if (totalMisses >= best_.misses)
                {
                    break;
                }
            }

            if (totalMisses < best_.misses)
            {
                best_ = {selectedCapacities, policyIndices, totalMisses};
                best_.capacities.push_back(capacity);
                best_.policyIndices.push_back(policyIndex);
            }
        }
    }

    void searchAdditionalLevels(const Traces_t& traces, std::size_t remainingCapacity,
                                int remainingLevels, std::vector<std::size_t>& selectedCapacities,
                                std::vector<int>& policyIndices)
    {
        if (remainingLevels <= 1 || best_.misses == theoreticalMinimum_)
        {
            return;
        }

        for (std::size_t capacity = 1; capacity < remainingCapacity; ++capacity)
        {
            std::vector<Traces_t> distinctMissTraces;

            for (int policyIndex = 0; policyIndex < static_cast<int>(policyNames.size());
                 ++policyIndex)
            {
                if (!isPolicySupported(policyIndex, capacity))
                {
                    continue;
                }

                if (best_.misses == theoreticalMinimum_)
                {
                    return;
                }

                Traces_t missTraces;

                for (const auto& trace : traces)
                {
                    missTraces.push_back(
                        {collectMissRequests(trace.requests, policyIndex, capacity),
                         trace.repetitionCount});
                }

                const auto hasSameRequests = [&missTraces](const Traces_t& previous)
                {
                    for (std::size_t index = 0; index < missTraces.size(); ++index)
                    {
                        if (previous[index].requests != missTraces[index].requests)
                        {
                            return false;
                        }
                    }

                    return true;
                };

                const bool isDuplicate = std::any_of(distinctMissTraces.begin(),
                                                     distinctMissTraces.end(), hasSameRequests);

                if (isDuplicate)
                {
                    ++prunedBranches_;
                    continue;
                }

                distinctMissTraces.push_back(missTraces);

                // All lower levels combined cannot beat MIN with their total capacity.
                if (lowerBound(missTraces, remainingCapacity - capacity) >= best_.misses)
                {
                    ++prunedBranches_;
                    continue;
                }

                selectedCapacities.push_back(capacity);
                policyIndices.push_back(policyIndex);

                evaluateLastLevel(missTraces, remainingCapacity - capacity, selectedCapacities,
                                  policyIndices);
                searchAdditionalLevels(missTraces, remainingCapacity - capacity,
                                       remainingLevels - 1, selectedCapacities, policyIndices);

                selectedCapacities.pop_back();
                policyIndices.pop_back();
            }
        }
    }

public:
    Configuration bestSingleLevel;

    ExactSearch(const Traces_t& traces, std::size_t totalCapacity, int maxLevels)
        : inputTraces_(traces), totalCapacity_(totalCapacity), maxLevels_(maxLevels),
          theoreticalMinimum_(lowerBound(traces, totalCapacity))
    {}

    Configuration run()
    {
        std::vector<std::size_t> selectedCapacities;
        std::vector<int> policyIndices;

        evaluateLastLevel(inputTraces_, totalCapacity_, selectedCapacities, policyIndices);
        bestSingleLevel = best_;

        searchAdditionalLevels(inputTraces_, totalCapacity_, maxLevels_, selectedCapacities,
                               policyIndices);

        return best_;
    }

    std::size_t referenceMisses() const
    {
        return theoreticalMinimum_;
    }

    std::size_t evaluatedCandidates() const
    {
        return evaluatedCandidates_;
    }

    std::size_t prunedBranches() const
    {
        return prunedBranches_;
    }
};

std::size_t replayConfiguration(const Requests_t& requests, const Configuration& configuration)
{
    Requests_t remainingRequests = requests;

    for (std::size_t level = 0; level < configuration.capacities.size(); ++level)
    {
        remainingRequests = collectMissRequests(
            remainingRequests, configuration.policyIndices[level], configuration.capacities[level]);
    }

    return remainingRequests.size();
}

void printConfiguration(const Configuration& configuration, const std::vector<Requests_t>& runs)
{
    std::cout << "{\"misses\":" << configuration.misses << ",\"capacities\":[";

    for (std::size_t index = 0; index < configuration.capacities.size(); ++index)
    {
        if (index != 0)
        {
            std::cout << ',';
        }

        std::cout << configuration.capacities[index];
    }

    std::cout << "],\"policies\":[";

    for (std::size_t index = 0; index < configuration.policyIndices.size(); ++index)
    {
        if (index != 0)
        {
            std::cout << ',';
        }

        std::cout << '\"' << policyNames[configuration.policyIndices[index]] << '\"';
    }

    std::cout << "],\"misses_by_run\":[";

    std::size_t totalMisses = 0;

    for (std::size_t index = 0; index < runs.size(); ++index)
    {
        if (index != 0)
        {
            std::cout << ',';
        }

        const auto misses = replayConfiguration(runs[index], configuration);
        totalMisses += misses;
        std::cout << misses;
    }

    if (totalMisses != configuration.misses)
    {
        throw std::runtime_error("Replay disagrees with search");
    }

    std::cout << "]}";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4)
        {
            throw std::invalid_argument("Usage: cache_exact_search traces.txt max_levels total...");
        }

        const int maxLevels = std::stoi(argv[2]);

        if (maxLevels < 1 || maxLevels > 3)
        {
            throw std::invalid_argument("Supported search depth: 1..3");
        }

        std::ifstream input(argv[1]);
        int runCount = 0;

        if (!(input >> runCount) || runCount < 1)
        {
            throw std::invalid_argument("Invalid trace file");
        }

        std::vector<Requests_t> runs;
        Traces_t uniqueTraces;

        for (int run = 0; run < runCount; ++run)
        {
            int requestCount = 0;

            if (!(input >> requestCount) || requestCount < 1)
            {
                throw std::invalid_argument("Empty or invalid trace");
            }

            Requests_t requests;

            for (int index = 0; index < requestCount; ++index)
            {
                int key = 0;

                if (!(input >> key))
                {
                    throw std::invalid_argument("Missing request");
                }

                requests.push_back(key);
            }

            runs.push_back(requests);

            auto found = std::find_if(uniqueTraces.begin(), uniqueTraces.end(),
                                      [&requests](const Trace& trace)
            { return trace.requests == requests; });

            if (found == uniqueTraces.end())
            {
                uniqueTraces.push_back({std::move(requests), 1});
            }
            else
            {
                ++found->repetitionCount;
            }
        }

        std::string extra;

        if (input >> extra)
        {
            throw std::invalid_argument("Trailing input");
        }

        for (int argument = 3; argument < argc; ++argument)
        {
            const int totalCapacity = std::stoi(argv[argument]);

            if (totalCapacity < 1)
            {
                throw std::invalid_argument("Total capacity must be positive");
            }

            ExactSearch search(uniqueTraces, static_cast<std::size_t>(totalCapacity), maxLevels);
            const auto best = search.run();

            std::cout << "{\"total\":" << totalCapacity
                      << ",\"reference_misses\":" << search.referenceMisses()
                      << ",\"evaluated\":" << search.evaluatedCandidates()
                      << ",\"pruned\":" << search.prunedBranches() << ",\"single\":";
            printConfiguration(search.bestSingleLevel, runs);
            std::cout << ",\"best\":";
            printConfiguration(best, runs);
            std::cout << "}\n" << std::flush;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Exact search failed: " << error.what() << '\n';

        return 1;
    }
}
