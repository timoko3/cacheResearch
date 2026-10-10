cmake_minimum_required(VERSION 3.24)
project(CacheConsumer LANGUAGES CXX)

# The library must raise the consumer's language requirement without changing
# the parent's setting or propagating developer compiler flags.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CACHE_RESEARCH_MODE STREQUAL "subdirectory")
    add_subdirectory("${CACHE_RESEARCH_SOURCE}" cache-library)
    foreach(option IN ITEMS BUILD_APP BUILD_BENCHMARKS BUILD_TESTS INSTALL)
        if(CACHE_RESEARCH_${option})
            message(FATAL_ERROR "Nested default CACHE_RESEARCH_${option} must be OFF")
        endif()
    endforeach()
    foreach(target IN ITEMS cache_research cache_tests cache_benchmark_runner cache_exact_search)
        if(TARGET ${target})
            message(FATAL_ERROR "Unrequested target: ${target}")
        endif()
    endforeach()
else()
    # Before 1.0, different minor versions are deliberately incompatible.
    find_package(CacheResearch 0.2 QUIET CONFIG)
    if(CacheResearch_FOUND)
        message(FATAL_ERROR "The 0.1 package must reject version 0.2")
    endif()
    find_package(CacheResearch 0.1.0 EXACT CONFIG REQUIRED)
    get_target_property(directories CacheResearch::cache INTERFACE_INCLUDE_DIRECTORIES)
    foreach(forbidden IN ITEMS CACHE_RESEARCH_FORBIDDEN_SOURCE CACHE_RESEARCH_FORBIDDEN_PREFIX)
        string(FIND "${directories}" "${${forbidden}}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR "Installed target refers to ${${forbidden}}")
        endif()
    endforeach()
endif()

if(NOT CMAKE_CXX_STANDARD EQUAL 11)
    message(FATAL_ERROR "The library changed the parent C++ standard")
endif()
get_target_property(flags CacheResearch::cache INTERFACE_COMPILE_OPTIONS)
get_target_property(dependencies CacheResearch::cache INTERFACE_LINK_LIBRARIES)
if(flags OR dependencies)
    message(FATAL_ERROR "The library propagates unexpected flags or dependencies")
endif()

add_executable(cache_consumer main.cpp)
target_link_libraries(cache_consumer PRIVATE CacheResearch::cache)
set_target_properties(cache_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>")

add_library(consumer_header_checks OBJECT
    cache_headers/base.cpp cache_headers/factory.cpp cache_headers/2Q.cpp
    cache_headers/ARC.cpp cache_headers/LFU.cpp cache_headers/LIRS.cpp
    cache_headers/LRU.cpp cache_headers/REF.cpp cache_headers/umbrella.cpp
    cache_headers/system.cpp)
target_link_libraries(consumer_header_checks PRIVATE CacheResearch::cache)
