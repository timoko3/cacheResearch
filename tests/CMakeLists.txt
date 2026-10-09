if(NOT TARGET GTest::gtest_main)
    include(FetchContent)
    # GoogleTest is a private test dependency, not part of our installation.
    set(INSTALL_GTEST OFF)
    set(BUILD_GMOCK OFF)
    if(MSVC)
        set(gtest_force_shared_crt ON)
    endif()
    FetchContent_Declare(googletest
        URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.18.0
        URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 60
        INACTIVITY_TIMEOUT 30
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_library(cache_header_checks OBJECT
    cache_headers/base.cpp
    cache_headers/factory.cpp
    cache_headers/2Q.cpp
    cache_headers/ARC.cpp
    cache_headers/LFU.cpp
    cache_headers/LIRS.cpp
    cache_headers/LRU.cpp
    cache_headers/REF.cpp
    cache_headers/umbrella.cpp
    cache_headers/system.cpp
)
target_link_libraries(cache_header_checks PRIVATE CacheResearch::cache)
cache_research_configure_target(cache_header_checks)

add_executable(cache_tests
    BASE_tests.cpp
    FACTORY_tests.cpp
    2Q_tests.cpp
    ARC_tests.cpp
    LFU_tests.cpp
    LIRS_tests.cpp
    LRU_tests.cpp
    REF_tests.cpp
    CACHE_SYS_tests.cpp
)
target_link_libraries(cache_tests PRIVATE CacheResearch::cache GTest::gtest_main)
cache_research_configure_target(cache_tests)
include(GoogleTest)
gtest_discover_tests(cache_tests DISCOVERY_MODE PRE_TEST)

if(TARGET CacheResearch::general_functions)
    add_executable(cache_tools_tests
        LEXER_tests.cpp TOKEN_tests.cpp TOKEN_STREAM_tests.cpp)
    target_link_libraries(cache_tools_tests PRIVATE
        CacheResearch::general_functions GTest::gtest_main)
    cache_research_configure_target(cache_tools_tests)
    gtest_discover_tests(cache_tools_tests DISCOVERY_MODE PRE_TEST)
endif()

# These consumers execute native binaries; cross builds need a separate
# toolchain/emulator-aware check rather than silently using the host.
if(NOT CMAKE_CROSSCOMPILING)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/VerificationContext.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/VerificationContext.cmake" @ONLY)
    foreach(mode IN ITEMS subdirectory installed)
        add_test(NAME "CachePackage.${mode}"
            COMMAND "${CMAKE_COMMAND}"
                "-DCACHE_RESEARCH_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                "-DCACHE_RESEARCH_BINARY_DIR=${PROJECT_BINARY_DIR}"
                "-DCACHE_RESEARCH_GENERATOR=${CMAKE_GENERATOR}"
                "-DCACHE_RESEARCH_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                "-DCACHE_RESEARCH_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
                "-DCACHE_RESEARCH_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
                "-DCACHE_RESEARCH_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                "-DCACHE_RESEARCH_CONFIG=$<CONFIG>"
                "-DCACHE_RESEARCH_MODE=${mode}"
                "-DCACHE_RESEARCH_CONTEXT=${CMAKE_CURRENT_BINARY_DIR}/VerificationContext.cmake"
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPackage.cmake"
        )
        set_tests_properties("CachePackage.${mode}" PROPERTIES TIMEOUT 180)
    endforeach()
endif()
