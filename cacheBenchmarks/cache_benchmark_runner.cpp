#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>

#include "cache/cacheREF.h"
#include "cacheParser.h"
#include "file.h"

void checkReadableFile(const std::string& filePath) {
    std::ifstream inputFile(filePath);

    if (!inputFile) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
}

void checkCacheConfiguration(const cache::CacheSystemParams& configuration) {
    if (configuration.levels.empty() || configuration.levels.size() > 3) {
        throw std::invalid_argument("Use 1 to 3 cache levels");
    }

    for (const auto& description : configuration.levels) {
        if (description.size == 0) {
            throw std::invalid_argument("Cache capacity must be positive");
        }

        if (description.strategy == cache::CacheEviction::C_REF &&
            configuration.levels.size() != 1) {
            throw std::invalid_argument("REF is supported only as a single-level reference");
        }
    }
}

uint32_t generatePageValue(int pageKey) {
    return static_cast<uint32_t>(pageKey) * 2654435761u;
}

template <typename CacheType>
std::size_t processRequests(CacheType& cacheInstance, const std::vector<int>& requests) {
    std::size_t memoryLoadCount = 0;

    uint32_t loadedPage = 0;
    auto slowGetPage = [&](int requestedKey) -> uint32_t& {
        ++memoryLoadCount;
        loadedPage = generatePageValue(requestedKey);
        return loadedPage;
    };

    for (const int requestedKey : requests) {
        if (cacheInstance.lookupUpdate(requestedKey, slowGetPage) !=
            generatePageValue(requestedKey)) {
            throw std::runtime_error("Incorrect cached page");
        }
    }

    return memoryLoadCount;
}

void checkCacheStatistics(const cache::CacheSystemStats& statistics, std::size_t requestCount,
                          std::size_t memoryLoadCount, std::size_t levelCount) {
    if (statistics.levels.size() != levelCount) {
        throw std::runtime_error("Incorrect number of cache levels");
    }

    std::size_t expectedRequests = requestCount;
    std::size_t totalHits = 0;

    for (const auto& level : statistics.levels) {
        if (level.stats.amountRequests != expectedRequests ||
            level.stats.amountHits + level.stats.amountMisses != expectedRequests) {
            throw std::runtime_error("Incorrect level statistics");
        }

        totalHits += level.stats.amountHits;
        expectedRequests = level.stats.amountMisses;
    }

    if (statistics.total.amountRequests != requestCount ||
        statistics.total.amountHits != totalHits ||
        statistics.total.amountMisses != memoryLoadCount || expectedRequests != memoryLoadCount ||
        totalHits + memoryLoadCount != requestCount) {
        throw std::runtime_error("Incorrect total statistics");
    }
}

cache::CacheSystemStats runCacheExperiment(const cache::CacheSystemParams& configuration,
                                           const std::vector<int>& requests) {
    checkCacheConfiguration(configuration);

    if (requests.empty()) {
        throw std::invalid_argument("Request list must not be empty");
    }

    cache::CacheSystemStats statistics{};
    std::size_t memoryLoadCount = 0;

    if (configuration.levels.front().strategy == cache::CacheEviction::C_REF) {
        std::list<int> futureRequests(requests.begin(), requests.end());
        cache::CacheREF<uint32_t, int> idealCache(configuration.levels.front().size,
                                                  futureRequests);

        memoryLoadCount = processRequests(idealCache, requests);
        statistics.total = idealCache.getStats();
        statistics.levels.push_back({configuration.levels.front().level, statistics.total});
    } else {
        cache::CacheSystem<uint32_t, int> cacheSystem(configuration);

        memoryLoadCount = processRequests(cacheSystem, requests);
        statistics = cacheSystem.getSystemStats();
    }

    checkCacheStatistics(statistics, requests.size(), memoryLoadCount, configuration.levels.size());

    return statistics;
}

void printCacheStatistics(const cache::CacheSystemStats& statistics) {
    std::cout << statistics.total.amountRequests << ' ' << statistics.total.amountHits << ' '
              << statistics.total.amountMisses;

    for (std::size_t levelIndex = 0; levelIndex < 3; ++levelIndex) {
        if (levelIndex < statistics.levels.size()) {
            const auto& levelStats = statistics.levels[levelIndex].stats;
            std::cout << ' ' << levelStats.amountRequests << ' ' << levelStats.amountHits << ' '
                      << levelStats.amountMisses;
        } else {
            std::cout << " 0 0 0";
        }
    }

    std::cout << '\n';
}

cache::CacheSystemStats runCacheExperimentFromFiles(const std::string& configPath,
                                                    const std::string& inputPath) {
    checkReadableFile(configPath);
    checkReadableFile(inputPath);

    lexer::Lexer configLexer(generalFunctions::readFile(configPath), configPath);
    lexer::Lexer inputLexer(generalFunctions::readFile(inputPath), inputPath);

    lexer::TokenStream tsConfig(configLexer.tokenize(), configPath);
    lexer::TokenStream tsInput(inputLexer.tokenize(), inputPath);

    parser::CacheParser<int> parser;
    parser.parseAll(tsConfig, tsInput);

    return runCacheExperiment(parser.getCacheSysParams(), parser.getReqList());
}

int main(int argc, char** argv) {
    try {
        // Each experiment owns fresh caches; batch mode only avoids process startup.
        if (argc == 2 && std::string(argv[1]) == "--batch") {
            std::string configPath;
            std::string inputPath;

            while (std::getline(std::cin, configPath)) {
                if (!std::getline(std::cin, inputPath)) {
                    throw std::invalid_argument("Missing input path in batch");
                }
                printCacheStatistics(runCacheExperimentFromFiles(configPath, inputPath));
                std::cout.flush();
            }
            return 0;
        }

        if (argc != 3) {
            throw std::invalid_argument("Usage: cache_benchmark_runner config.txt input.txt");
        }

        printCacheStatistics(runCacheExperimentFromFiles(argv[1], argv[2]));
    } catch (const std::exception& benchmarkError) {
        std::cerr << "Benchmark failed: " << benchmarkError.what() << '\n';
        return 1;
    }

    return 0;
}
