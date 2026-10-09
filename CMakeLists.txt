cmake_minimum_required(VERSION 3.24)
# Check this project's directories, including when used by add_subdirectory.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use a separate build directory.")
endif()

project(CacheResearch VERSION 0.1.0 LANGUAGES CXX)

option(CACHE_RESEARCH_BUILD_APP "Build the research application" OFF)
option(CACHE_RESEARCH_BUILD_BENCHMARKS "Build the C++ benchmark runners" OFF)
option(CACHE_RESEARCH_BUILD_TESTS "Build and register project tests" OFF)
option(CACHE_RESEARCH_INSTALL "Generate installation rules for the cache library" "${PROJECT_IS_TOP_LEVEL}")

# The cache library has no compiled sources or third-party dependencies.
add_library(cache_research_cache INTERFACE)
add_library(CacheResearch::cache ALIAS cache_research_cache)
set_target_properties(cache_research_cache PROPERTIES EXPORT_NAME cache)
target_compile_features(cache_research_cache INTERFACE cxx_std_17)
target_sources(cache_research_cache INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        cache.h
        cacheSystem.h
        cache/cache.h
        cache/cache2Q.h
        cache/cacheARC.h
        cache/cacheFactory.h
        cache/cacheLFU.h
        cache/cacheLIRS.h
        cache/cacheLRU.h
        cache/cacheREF.h
)

if(CACHE_RESEARCH_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(cache_research_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CacheResearch")
    install(TARGETS cache_research_cache EXPORT CacheResearchTargets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CacheResearchTargets
        FILE CacheResearchTargets.cmake
        NAMESPACE CacheResearch::
        DESTINATION "${cache_research_package_dir}"
    )
    configure_package_config_file(cmake/CacheResearchConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/CacheResearchConfig.cmake"
        INSTALL_DESTINATION "${cache_research_package_dir}"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CacheResearchConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
        ARCH_INDEPENDENT
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CacheResearchConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CacheResearchConfigVersion.cmake"
        DESTINATION "${cache_research_package_dir}"
    )
endif()

# Developer settings are private; consumers inherit only headers and C++17.
function(cache_research_configure_target target)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Only tools need generalFunctions sources. Library tests need just GoogleTest.
if(CACHE_RESEARCH_BUILD_APP OR CACHE_RESEARCH_BUILD_BENCHMARKS)
    set(cache_research_general_default "")
    if(DEFINED ENV{GENERAL_FUNCTIONS_SOURCE_DIR})
        set(cache_research_general_default "$ENV{GENERAL_FUNCTIONS_SOURCE_DIR}")
    endif()
    set(CACHE_RESEARCH_GENERAL_FUNCTIONS_SOURCE_DIR "${cache_research_general_default}"
        CACHE PATH "Path to generalFunctions sources (empty downloads the pinned archive)")
    set(cache_research_general_dir "${CACHE_RESEARCH_GENERAL_FUNCTIONS_SOURCE_DIR}")
    if(NOT cache_research_general_dir)
        include(FetchContent)
        FetchContent_Declare(generalfunctions
            URL https://codeload.github.com/timoko3/generalFunctions/tar.gz/dc31aef3c60cb88428c6aa7b150edbc36028508e
            URL_HASH SHA256=96c500d80057f398875894dd79226001e857e9e0b291c4c29ca1bd0ae47156c1
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            TIMEOUT 60
            INACTIVITY_TIMEOUT 30
        )
        FetchContent_MakeAvailable(generalfunctions)
        set(cache_research_general_dir "${generalfunctions_SOURCE_DIR}")
    endif()
    set(cache_research_general_sources
        file.cpp
        file.h
        lexer/lexer.cpp
        lexer/lexer.h
        lexer/token_stream.cpp
        lexer/token_stream.h
        lexer/token.h
    )
    list(TRANSFORM cache_research_general_sources
        PREPEND "${cache_research_general_dir}/")
    foreach(source IN LISTS cache_research_general_sources)
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR
                "Missing ${source}. Tools require generalFunctions sources. "
                "Set CACHE_RESEARCH_GENERAL_FUNCTIONS_SOURCE_DIR, enter nix develop, "
                "allow FetchContent to download it, or disable "
                "CACHE_RESEARCH_BUILD_APP and CACHE_RESEARCH_BUILD_BENCHMARKS."
            )
        endif()
    endforeach()
    add_library(cache_research_general_functions STATIC ${cache_research_general_sources})
    add_library(CacheResearch::general_functions ALIAS cache_research_general_functions)
    target_compile_features(cache_research_general_functions PUBLIC cxx_std_17)
    target_include_directories(cache_research_general_functions PUBLIC
        "${cache_research_general_dir}")
    cache_research_configure_target(cache_research_general_functions)
endif()

if(CACHE_RESEARCH_BUILD_APP)
    add_executable(cache_research main.cpp cacheParser.h)
    target_link_libraries(cache_research PRIVATE
        CacheResearch::cache CacheResearch::general_functions)
    cache_research_configure_target(cache_research)

    set_target_properties(cache_research PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>")
    if(CACHE_RESEARCH_INSTALL)
        install(TARGETS cache_research RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
endif()

if(CACHE_RESEARCH_BUILD_BENCHMARKS)
    add_executable(cache_exact_search cacheBenchmarks/cache_exact_search.cpp)
    target_link_libraries(cache_exact_search PRIVATE CacheResearch::cache)
    cache_research_configure_target(cache_exact_search)

    add_executable(cache_benchmark_runner cacheBenchmarks/cache_benchmark_runner.cpp)
    target_link_libraries(cache_benchmark_runner PRIVATE
        CacheResearch::cache CacheResearch::general_functions)
    cache_research_configure_target(cache_benchmark_runner)
endif()

# BUILD_TESTING=OFF is a global opt-out, including for an explicitly enabled preset.
if(CACHE_RESEARCH_BUILD_TESTS AND (NOT DEFINED BUILD_TESTING OR BUILD_TESTING))
    enable_testing()
    add_subdirectory(tests)
endif()
