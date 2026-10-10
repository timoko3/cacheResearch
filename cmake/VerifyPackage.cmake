# Exercise a reduced source tree: no submodules, runtime data, or research tools.
foreach(required IN ITEMS SOURCE_DIR BINARY_DIR GENERATOR CXX_COMPILER)
    if(NOT CACHE_RESEARCH_${required})
        message(FATAL_ERROR "Missing CACHE_RESEARCH_${required}")
    endif()
endforeach()
if(NOT CACHE_RESEARCH_MODE MATCHES "^(subdirectory|installed)$")
    message(FATAL_ERROR "Unknown package verification mode: ${CACHE_RESEARCH_MODE}")
endif()

function(run_checked)
    execute_process(COMMAND ${ARGV}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
    endif()
endfunction()

set(config "${CACHE_RESEARCH_CONFIG}")
if(NOT config)
    set(config Debug)
endif()
if(NOT config MATCHES "^[A-Za-z0-9_ .+-]+$" OR config STREQUAL "." OR config STREQUAL "..")
    message(FATAL_ERROR "Unsafe configuration name: ${config}")
endif()
# Keep each configuration isolated, and reject any cleanup outside this build tree.
file(REAL_PATH "${CACHE_RESEARCH_BINARY_DIR}" verification_root)
set(work "${verification_root}/package-check-${CACHE_RESEARCH_MODE}-${config} with spaces")
file(REAL_PATH "${work}" resolved_work)
cmake_path(IS_PREFIX verification_root "${resolved_work}" NORMALIZE inside_build)
if(NOT inside_build OR resolved_work STREQUAL verification_root)
    message(FATAL_ERROR "Unsafe verification directory: ${resolved_work}")
endif()
file(REMOVE_RECURSE "${work}")
if(EXISTS "${work}")
    message(FATAL_ERROR "Cannot clean verification directory: ${work}")
endif()
set(library_source "${work}/library source")
set(consumer_source "${work}/consumer source")
file(MAKE_DIRECTORY "${library_source}/cmake" "${consumer_source}")
file(COPY
    "${CACHE_RESEARCH_SOURCE_DIR}/CMakeLists.txt"
    "${CACHE_RESEARCH_SOURCE_DIR}/cache.h"
    "${CACHE_RESEARCH_SOURCE_DIR}/cacheSystem.h"
    "${CACHE_RESEARCH_SOURCE_DIR}/cache"
    DESTINATION "${library_source}")
file(COPY "${CACHE_RESEARCH_SOURCE_DIR}/cmake/CacheResearchConfig.cmake.in"
    DESTINATION "${library_source}/cmake")
file(COPY "${CACHE_RESEARCH_SOURCE_DIR}/tests/cmake_consumer/"
    DESTINATION "${consumer_source}")
file(COPY "${CACHE_RESEARCH_SOURCE_DIR}/tests/cache_headers"
    DESTINATION "${consumer_source}")

set(configure_args
    -G "${CACHE_RESEARCH_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CACHE_RESEARCH_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${config}")
if(CACHE_RESEARCH_CONTEXT)
    list(APPEND configure_args -C "${CACHE_RESEARCH_CONTEXT}")
endif()
if(CACHE_RESEARCH_MAKE_PROGRAM)
    list(APPEND configure_args "-DCMAKE_MAKE_PROGRAM=${CACHE_RESEARCH_MAKE_PROGRAM}")
endif()
if(CACHE_RESEARCH_GENERATOR_PLATFORM)
    list(APPEND configure_args -A "${CACHE_RESEARCH_GENERATOR_PLATFORM}")
endif()
if(CACHE_RESEARCH_GENERATOR_TOOLSET)
    list(APPEND configure_args -T "${CACHE_RESEARCH_GENERATOR_TOOLSET}")
endif()

set(consumer_args "-DCACHE_RESEARCH_MODE=${CACHE_RESEARCH_MODE}")
if(CACHE_RESEARCH_MODE STREQUAL "subdirectory")
    list(APPEND consumer_args "-DCACHE_RESEARCH_SOURCE=${library_source}")
else()
    set(library_build "${work}/library build")
    set(prefix "${work}/original prefix")
    set(relocated_prefix "${work}/relocated prefix")
    run_checked("${CMAKE_COMMAND}" -S "${library_source}" -B "${library_build}"
        ${configure_args}
        -DCACHE_RESEARCH_BUILD_APP=OFF
        -DCACHE_RESEARCH_BUILD_BENCHMARKS=OFF
        -DCACHE_RESEARCH_BUILD_TESTS=OFF
        -DCACHE_RESEARCH_INSTALL=ON
        "-DCMAKE_INSTALL_PREFIX=${prefix}"
        -DCMAKE_INSTALL_INCLUDEDIR=include/custom
        -DCMAKE_INSTALL_LIBDIR=lib)
    run_checked("${CMAKE_COMMAND}" --build "${library_build}" --config "${config}")
    run_checked("${CMAKE_COMMAND}" --install "${library_build}" --config "${config}")
    file(COPY "${prefix}/" DESTINATION "${relocated_prefix}")
    # The relocated consumer must work after both original headers and prefix disappear.
    file(REMOVE_RECURSE "${prefix}" "${library_source}" "${library_build}")
    if(EXISTS "${prefix}" OR EXISTS "${library_source}" OR EXISTS "${library_build}")
        message(FATAL_ERROR "Cannot isolate the relocated package from its original files")
    endif()
    list(APPEND consumer_args
        "-DCMAKE_PREFIX_PATH=${relocated_prefix}"
        "-DCACHE_RESEARCH_FORBIDDEN_SOURCE=${library_source}"
        "-DCACHE_RESEARCH_FORBIDDEN_PREFIX=${prefix}")
endif()

set(consumer_build "${work}/consumer build")
run_checked("${CMAKE_COMMAND}" -S "${consumer_source}" -B "${consumer_build}"
    ${configure_args} ${consumer_args})
run_checked("${CMAKE_COMMAND}" --build "${consumer_build}" --config "${config}" --parallel 2)
if(CMAKE_HOST_WIN32)
    set(executable_suffix .exe)
else()
    set(executable_suffix "")
endif()
run_checked("${consumer_build}/bin/${config}/cache_consumer${executable_suffix}")
