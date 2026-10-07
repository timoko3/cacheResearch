cmake_minimum_required(VERSION 3.24)

# Keep generated files out of the source tree.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use cmake --preset debug to create a build directory for this OS.")
endif()

# Require configuration and input files without prescribing their names.
set(empty_runtime_dirs "")
foreach(runtime_dir IN ITEMS configs inputs)
    file(GLOB runtime_files LIST_DIRECTORIES false CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${runtime_dir}/*")
    if(NOT runtime_files)
        list(APPEND empty_runtime_dirs "${runtime_dir}/")
    endif()
endforeach()
if(empty_runtime_dirs)
    list(JOIN empty_runtime_dirs ", " empty_runtime_dirs_text)
    message(FATAL_ERROR
        "No configuration/input files found in: ${empty_runtime_dirs_text}\n"
        "Create these directories in the project root and add at least one file "
        "to each. File names and extensions are unrestricted.\n"
        "The config file contains the level count and cache strategies; "
        "the input file contains level capacities, request count and requests.\n"
        "See README.md, section 6, for examples.")
endif()

project(CacheResearch LANGUAGES CXX)

option(CACHE_RESEARCH_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)

# The submodule has no CMake project of its own. Describe its target here,
# without modifying the submodule or fetching anything during configuration.
foreach(source IN ITEMS file.cpp file.h logger.cpp logger.h mathVector.h)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generalFunctions/${source}")
        message(FATAL_ERROR
            "Missing generalFunctions/${source}. Initialize the submodule with: "
            "git submodule update --init --recursive")
    endif()
endforeach()

add_library(general_functions STATIC
    generalFunctions/file.cpp
    generalFunctions/file.h
    generalFunctions/lexer_new.cpp
    generalFunctions/lexer_new.h
    generalFunctions/logger.cpp
    generalFunctions/logger.h
    generalFunctions/mathVector.h
)
add_library(CacheResearch::general_functions ALIAS general_functions)

# Public headers use C++17 (filesystem/string_view), so consumers need it too.
target_compile_features(general_functions PUBLIC cxx_std_17)
target_include_directories(general_functions PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(cache_research
    main.cpp

    cache.h
    cacheSystem.h
    #cacheParser.h
)
target_compile_features(cache_research PRIVATE cxx_std_17)
target_link_libraries(cache_research PRIVATE CacheResearch::general_functions)

# Avoid global compiler flags: these settings belong only to these targets.
foreach(target IN ITEMS general_functions cache_research)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR "${CACHE_RESEARCH_WARNINGS_AS_ERRORS}"
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# $<CONFIG> works with both Ninja and multi-configuration generators (VS).
set(cache_research_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>")
set_target_properties(cache_research PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${cache_research_runtime_dir}"
)

# Refresh the runtime file even when only config.txt changed, without relinking.
add_custom_target(cache_research_pattern
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${cache_research_runtime_dir}/configs"

    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${cache_research_runtime_dir}/inputs"

    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/configs"
        "${cache_research_runtime_dir}/configs"

    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/inputs"
        "${cache_research_runtime_dir}/inputs"

    COMMENT "Updating runtime pattern"
    VERBATIM
)
add_dependencies(cache_research cache_research_pattern)

# Relative paths inside the application resolve from its runtime directory.
add_custom_target(run
    COMMAND "$<TARGET_FILE:cache_research>" configs/config_1.txt inputs/input_1.txt
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:cache_research>"
    DEPENDS cache_research
    USES_TERMINAL
    VERBATIM
)

enable_testing()

add_executable(cache_benchmark_runner EXCLUDE_FROM_ALL
    #cacheBenchmarks/cache_benchmark_runner.cpp
    generalFunctions/lexer_new.cpp
)

target_compile_features(cache_benchmark_runner PRIVATE cxx_std_17)

target_link_libraries(cache_benchmark_runner PRIVATE
    CacheResearch::general_functions
)

add_subdirectory(third_party/googletest)

file(GLOB cache_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*_tests.cpp"
)

add_executable(cache_tests EXCLUDE_FROM_ALL ${cache_test_sources} generalFunctions/lexer_new.cpp)

target_link_libraries(cache_tests PRIVATE
    CacheResearch::general_functions
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cache_tests)
