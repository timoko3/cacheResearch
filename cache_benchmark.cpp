#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "cacheSystem.h"

namespace benchmark {

const std::size_t kRequestsPerTrace = 20000;
const std::vector<unsigned> kRandomSeeds{1, 2, 3};

const std::vector<cache::cacheLevel> kCacheLevels{cache::L1, cache::L2, cache::L3};
const std::vector<double> kLevelAccessCosts{1.0, 5.0, 20.0};

const double kMemoryAccessCost = 100.0;

const std::size_t kTotalCacheCapacity = 64;
const std::vector<std::vector<std::size_t>> kPageCacheDistribution{
    {64},
    {8, 56},
    {16, 48},
    {32, 32},
    {8, 16, 40}
};


enum RequestPattern {
    SmallWorkingSetCycle,
    CapacityBoundaryCycle,
    SequentialScan,
    HotColdAccess,
    HotPagesWithScan,
    ChangingHotPages,
    RepeatedPageBursts,
    UniformRandomAccess
};

struct RequestPatternDescription {
    RequestPattern patternType;
    std::string patternName;
};

const std::vector<RequestPatternDescription> kRequestPatterns{
    {SmallWorkingSetCycle, "small_cycle"},
    {CapacityBoundaryCycle, "boundary_cycle"},
    {SequentialScan, "scan"},
    {HotColdAccess, "hot_cold"},
    {HotPagesWithScan, "hot_scan"},
    {ChangingHotPages, "phase_change"},
    {RepeatedPageBursts, "bursts"},
    {UniformRandomAccess, "uniform"}
};


int generateRandomNumber(std::mt19937& randomGenerator,
                         int minimumValue,
                         int maximumValue) {

    std::uniform_int_distribution<int> numberDistribution(minimumValue, maximumValue);

    return numberDistribution(randomGenerator);
}

int generateHotColdKey(std::mt19937& randomGenerator, int hotPageOffset) {
    if (generateRandomNumber(randomGenerator, 0, 99) < 90) {
        return hotPageOffset + generateRandomNumber(randomGenerator, 0, 7);
    }

    return generateRandomNumber(randomGenerator, 1024, 2047);
}

int generateRequestKey(RequestPattern patternType,
                       std::size_t requestIndex,
                       int previousRequestKey,
                       std::mt19937& randomGenerator) {

    switch (patternType) {
        case SmallWorkingSetCycle:
            return static_cast<int>(requestIndex % 32);

        case CapacityBoundaryCycle:
            return static_cast<int>(requestIndex % 65);

        case SequentialScan:
            return static_cast<int>(requestIndex % 256);

        case HotColdAccess:
            return generateHotColdKey(randomGenerator, 0);

        case HotPagesWithScan:
            if (requestIndex % 512 < 384) {
                return generateRandomNumber(randomGenerator, 0, 7);
            }

            return 1024 + static_cast<int>(requestIndex);

        case ChangingHotPages:
            return generateHotColdKey(
                randomGenerator, static_cast<int>((requestIndex / 2000) % 4) *
                                 static_cast<int>(kTotalCacheCapacity));

        case RepeatedPageBursts:
            if (requestIndex % 16 == 0) {
                return generateRandomNumber(randomGenerator, 0, 255);
            }

            return previousRequestKey;

        case UniformRandomAccess:
            return generateRandomNumber(randomGenerator, 0, 255);

        default:
            throw std::invalid_argument("Unknown pattern");
    }
}

std::vector<int> generateRequestSequence(RequestPattern patternType, unsigned randomSeed) {
    std::mt19937 randomGenerator(randomSeed);
    std::vector<int> requestSequence;
    requestSequence.reserve(kRequestsPerTrace);

    int previousRequestKey = 0;

    for (std::size_t requestIndex = 0; requestIndex < kRequestsPerTrace; ++requestIndex) {
        const int requestedKey = generateRequestKey(
            patternType, requestIndex, previousRequestKey, randomGenerator);

        requestSequence.push_back(requestedKey);
        previousRequestKey = requestedKey;
    }

    return requestSequence;
}


std::vector<cache::cacheDescription> createLevelVariants(std::size_t levelCapacity,
                                                       cache::cacheLevel cacheLevel) {

    std::vector<cache::cacheDescription> levelVariants(5);

    levelVariants[0].strategy = cache::C_LRU;
    levelVariants[1].strategy = cache::C_LFU;
    levelVariants[2].strategy = cache::C_ARC;
    levelVariants[3].strategy = cache::C_2Q;
    levelVariants[4].strategy = cache::C_LIRS;

    for (auto& levelDescription : levelVariants) {
        levelDescription.size = levelCapacity;
        levelDescription.level = cacheLevel;
    }

    return levelVariants;
}

std::vector<cache::cacheSystemParams> appendCacheLevel(
    const std::vector<cache::cacheSystemParams>& cacheConfigurations,
    std::size_t levelCapacity,
    cache::cacheLevel cacheLevel) {

    std::vector<cache::cacheSystemParams> expandedConfigurations;

    for (const auto& levelDescription : createLevelVariants(levelCapacity, cacheLevel)) {
        for (const auto& configuration : cacheConfigurations) {
            cache::cacheSystemParams extendedConfiguration = configuration;
            extendedConfiguration.levels.push_back(levelDescription);

            expandedConfigurations.push_back(extendedConfiguration);
        }
    }

    return expandedConfigurations;
}

void checkLevelCapacities(const std::vector<std::size_t>& levelCapacities) {
    if (levelCapacities.empty() || levelCapacities.size() > kCacheLevels.size()) {
        throw std::invalid_argument("Use 1 to 3 levels");
    }

    std::size_t totalCapacity = 0;

    for (const auto levelCapacity : levelCapacities) {
        if (levelCapacity < 4) {
            throw std::invalid_argument("Use at least 4 slots per level");
        }

        totalCapacity += levelCapacity;
    }

    if (totalCapacity != kTotalCacheCapacity) {
        throw std::invalid_argument("The total cache capacity must be 64");
    }
}

std::vector<cache::cacheSystemParams> createCacheConfigurations() {
    std::vector<cache::cacheSystemParams> allConfigurations;

    for (const auto& levelCapacities : kPageCacheDistribution) {
        checkLevelCapacities(levelCapacities);

        std::vector<cache::cacheSystemParams> cacheConfigurations(1);

        for (std::size_t levelIndex = 0; levelIndex < levelCapacities.size(); ++levelIndex) {
            cacheConfigurations = appendCacheLevel(
                cacheConfigurations, levelCapacities[levelIndex], kCacheLevels[levelIndex]);
        }

        for (const auto& configuration : cacheConfigurations) {
            allConfigurations.push_back(configuration);
        }
    }

    return allConfigurations;
}

std::string getConfigurationName(const cache::cacheSystemParams& configuration) {
    std::string configurationName;

    for (const auto& levelDescription : configuration.levels) {
        if (!configurationName.empty()) {
            configurationName += "+";
        }

        switch (levelDescription.strategy) {
            case cache::C_LRU:
                configurationName += "LRU";
                break;

            case cache::C_LFU:
                configurationName += "LFU";
                break;

            case cache::C_ARC:
                configurationName += "ARC";
                break;

            case cache::C_2Q:
                configurationName += "2Q";
                break;

            case cache::C_LIRS:
                configurationName += "LIRS";
                break;

            default:
                throw std::invalid_argument("Unknown strategy");
        }

        configurationName += std::to_string(levelDescription.size);
    }

    return configurationName;
}


uint32_t generatePageValue(int pageKey) {
    return static_cast<uint32_t>(pageKey) * 2654435761u;
}

void checkSystemStatistics(const cache::CacheSystemStats& systemStatistics,
                           std::size_t requestCount,
                           std::size_t memoryLoadCount,
                           std::size_t levelCount) {

    if (systemStatistics.levels.size() != levelCount) {
        throw std::runtime_error("Wrong level count");
    }

    std::size_t expectedLevelRequests = requestCount;
    std::size_t totalHitCount = 0;

    for (const auto& level : systemStatistics.levels) {
        const auto& levelStatistics = level.stats;

        if (levelStatistics.amountRequests != expectedLevelRequests ||
            levelStatistics.amountHits + levelStatistics.amountMisses != expectedLevelRequests) {
            throw std::runtime_error("Incorrect level statistics");
        }

        totalHitCount += levelStatistics.amountHits;
        expectedLevelRequests = levelStatistics.amountMisses;
    }

    if (expectedLevelRequests != memoryLoadCount ||
        totalHitCount + memoryLoadCount != requestCount ||
        systemStatistics.total.amountRequests != requestCount ||
        systemStatistics.total.amountHits != totalHitCount ||
        systemStatistics.total.amountMisses != memoryLoadCount) {
        throw std::runtime_error("Incorrect total statistics");
    }
}

cache::CacheSystemStats runCacheExperiment(const cache::cacheSystemParams& configuration,
                                          const std::vector<int>& requestSequence) {

    if (requestSequence.empty()) {
        throw std::invalid_argument("Empty request list");
    }

    cache::CacheSystem<uint32_t, int> cacheSystem(configuration);
    std::size_t memoryLoadCount = 0;

    auto slowGetPage = [&](int requestedKey) {
        ++memoryLoadCount;
        return generatePageValue(requestedKey);
    };

    for (const int requestedKey : requestSequence) {
        if (cacheSystem.lookupUpdate(requestedKey, slowGetPage) != generatePageValue(requestedKey)) {
            throw std::runtime_error("Incorrect page");
        }
    }

    const auto systemStatistics = cacheSystem.getStats();

    checkSystemStatistics(
        systemStatistics, requestSequence.size(), memoryLoadCount, configuration.levels.size());

    return systemStatistics;
}

double calculateAverageAccessCost(const cache::CacheSystemStats& systemStatistics) {
    double totalAccessCost = systemStatistics.total.amountMisses * kMemoryAccessCost;

    for (std::size_t levelIndex = 0; levelIndex < systemStatistics.levels.size(); ++levelIndex) {
        totalAccessCost += systemStatistics.levels[levelIndex].stats.amountRequests *
                           kLevelAccessCosts[levelIndex];
    }

    return totalAccessCost / systemStatistics.total.amountRequests;
}

double calculateHitRate(const cache::CacheSystemStats& systemStatistics) {
    return static_cast<double>(systemStatistics.total.amountHits) /
           systemStatistics.total.amountRequests;
}


void writeCsvHeader(std::ostream& csvOutput) {
    csvOutput << std::setprecision(12)
              << "pattern,seed,configuration,requests,"
                 "l1_requests,l1_hits,l1_misses,l2_requests,l2_hits,l2_misses,"
                 "l3_requests,l3_hits,l3_misses,memory_misses,hit_rate,amat\n";
}

void writeCsvResult(std::ostream& csvOutput,
                    const std::string& patternName,
                    unsigned randomSeed,
                    const cache::cacheSystemParams& configuration,
                    const cache::CacheSystemStats& systemStatistics) {

    csvOutput << patternName << ',' << randomSeed << ','
              << getConfigurationName(configuration) << ','
              << systemStatistics.total.amountRequests;

    for (std::size_t levelIndex = 0; levelIndex < kCacheLevels.size(); ++levelIndex) {
        if (levelIndex < systemStatistics.levels.size()) {
            const auto& levelStatistics = systemStatistics.levels[levelIndex].stats;

            csvOutput << ',' << levelStatistics.amountRequests
                      << ',' << levelStatistics.amountHits
                      << ',' << levelStatistics.amountMisses;
        } else {
            csvOutput << ",0,0,0";
        }
    }

    csvOutput << ',' << systemStatistics.total.amountMisses
              << ',' << calculateHitRate(systemStatistics)
              << ',' << calculateAverageAccessCost(systemStatistics) << '\n';
}


std::vector<std::size_t> rankConfigurationScores(const std::vector<double>& configurationScores) {
    std::vector<std::size_t> rankedIndices;
    std::vector<bool> selectedConfigurations(configurationScores.size(), false);

    for (std::size_t rankIndex = 0; rankIndex < configurationScores.size(); ++rankIndex) {
        std::size_t bestConfigurationIndex = configurationScores.size();

        for (std::size_t configurationIndex = 0;
             configurationIndex < configurationScores.size();
             ++configurationIndex) {

            if (!selectedConfigurations[configurationIndex] &&
                (bestConfigurationIndex == configurationScores.size() ||
                 configurationScores[configurationIndex] < configurationScores[bestConfigurationIndex])) {
                bestConfigurationIndex = configurationIndex;
            }
        }

        selectedConfigurations[bestConfigurationIndex] = true;
        rankedIndices.push_back(bestConfigurationIndex);
    }

    return rankedIndices;
}

void printBestConfigurations(const std::vector<cache::cacheSystemParams>& cacheConfigurations,
                             const std::vector<double>& configurationScores,
                             const std::string& metricName,
                             std::size_t resultCount,
                             const std::vector<double>& meanHitRates) {

    const auto rankedIndices = rankConfigurationScores(configurationScores);

    for (std::size_t rankIndex = 0;
         rankIndex < resultCount && rankIndex < rankedIndices.size();
         ++rankIndex) {

        const auto configurationIndex = rankedIndices[rankIndex];

        std::cout << "  " << rankIndex + 1 << ". "
                  << getConfigurationName(cacheConfigurations[configurationIndex])
                  << ' ' << metricName << '=' << configurationScores[configurationIndex];

        if (!meanHitRates.empty()) {
            std::cout << " hit=" << 100 * meanHitRates[configurationIndex] << '%';
        }

        std::cout << '\n';
    }

    for (const auto configurationIndex : rankedIndices) {
        if (cacheConfigurations[configurationIndex].levels.size() > 1) {
            std::cout << "  Best multi-level: "
                      << getConfigurationName(cacheConfigurations[configurationIndex])
                      << ' ' << metricName << '=' << configurationScores[configurationIndex] << '\n';
            break;
        }
    }
}


std::vector<double> evaluateRequestPattern(
    const RequestPatternDescription& requestPattern,
    const std::vector<cache::cacheSystemParams>& cacheConfigurations,
    std::ostream& csvOutput) {

    std::cout << '\n' << requestPattern.patternName << " ...\n" << std::flush;

    std::vector<double> meanAccessCosts(cacheConfigurations.size(), 0.0);
    std::vector<double> meanHitRates(cacheConfigurations.size(), 0.0);

    for (const auto randomSeed : kRandomSeeds) {
        const auto requestSequence = generateRequestSequence(requestPattern.patternType, randomSeed);

        for (std::size_t configurationIndex = 0;
             configurationIndex < cacheConfigurations.size();
             ++configurationIndex) {

            try {
                const auto systemStatistics = runCacheExperiment(
                    cacheConfigurations[configurationIndex], requestSequence);

                meanAccessCosts[configurationIndex] +=
                    calculateAverageAccessCost(systemStatistics) / kRandomSeeds.size();

                meanHitRates[configurationIndex] +=
                    calculateHitRate(systemStatistics) / kRandomSeeds.size();

                writeCsvResult(csvOutput, requestPattern.patternName, randomSeed,
                               cacheConfigurations[configurationIndex], systemStatistics);
            } catch (const std::exception& experimentError) {
                throw std::runtime_error(
                    requestPattern.patternName + ", seed=" + std::to_string(randomSeed) +
                    ", " + getConfigurationName(cacheConfigurations[configurationIndex]) +
                    ": " + experimentError.what());
            }
        }
    }

    printBestConfigurations(cacheConfigurations, meanAccessCosts, "AMAT", 3, meanHitRates);

    return meanAccessCosts;
}

void updateAverageSlowdown(const std::vector<double>& meanAccessCosts,
                           std::vector<double>& meanSlowdownPercent) {

    double minimumPatternCost = meanAccessCosts[0];

    for (const double meanAccessCost : meanAccessCosts) {
        if (meanAccessCost < minimumPatternCost) {
            minimumPatternCost = meanAccessCost;
        }
    }

    for (std::size_t configurationIndex = 0;
         configurationIndex < meanAccessCosts.size();
         ++configurationIndex) {

        meanSlowdownPercent[configurationIndex] +=
            100 * (meanAccessCosts[configurationIndex] / minimumPatternCost - 1) /
            kRequestPatterns.size();
    }
}

void runCacheBenchmark(const std::string& outputFilePath) {
    std::ofstream csvOutput(outputFilePath);

    if (!csvOutput) {
        throw std::runtime_error("Cannot open CSV: " + outputFilePath);
    }

    writeCsvHeader(csvOutput);

    const auto cacheConfigurations = createCacheConfigurations();
    std::vector<double> meanSlowdownPercent(cacheConfigurations.size(), 0.0);

    std::cout << std::fixed << std::setprecision(3)
              << cacheConfigurations.size() << " configurations, "
              << kRequestsPerTrace << " requests, "
              << kRandomSeeds.size() << " seeds; cold start included.\n";

    for (const auto& requestPattern : kRequestPatterns) {
        const auto meanAccessCosts = evaluateRequestPattern(
            requestPattern, cacheConfigurations, csvOutput);

        updateAverageSlowdown(meanAccessCosts, meanSlowdownPercent);
    }

    std::cout << "\nTop 5 compromises: mean slowdown (%) vs each pattern's best\n";
    printBestConfigurations(cacheConfigurations, meanSlowdownPercent, "slowdown(%)", 5, {});

    csvOutput.close();

    if (!csvOutput) {
        throw std::runtime_error("Failed to write CSV: " + outputFilePath);
    }

    std::cout << "\nCSV: " << outputFilePath
              << "\nBest among these candidates under this cost model.\n";
}

}


int main(int argc, char** argv) {
    try {
        if (argc > 2) {
            throw std::invalid_argument("Usage: cache_benchmark [output.csv]");
        }

        std::string outputFilePath = "cache_benchmark_results.csv";

        if (argc == 2) {
            outputFilePath = argv[1];
        }

        benchmark::runCacheBenchmark(outputFilePath);
    } catch (const std::exception& benchmarkError) {
        std::cerr << "Benchmark failed: " << benchmarkError.what() << '\n';
        return 1;
    }

    return 0;
}
