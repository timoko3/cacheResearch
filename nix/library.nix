{ lib, stdenv, cmake, ninja, src ? lib.cleanSource ../. }:

stdenv.mkDerivation {
  pname = "cache-research-library";
  version = "0.1.0";
  inherit src;

  nativeBuildInputs = [ cmake ninja ];
  cmakeFlags = [
    "-DCACHE_RESEARCH_BUILD_APP=OFF"
    "-DCACHE_RESEARCH_BUILD_BENCHMARKS=OFF"
    "-DCACHE_RESEARCH_BUILD_TESTS=OFF"
    "-DCACHE_RESEARCH_INSTALL=ON"
    "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
  ];

  # Compile and run a consumer against the installed headers and CMake export.
  doInstallCheck = stdenv.buildPlatform.canExecute stdenv.hostPlatform;
  installCheckPhase = ''
    runHook preInstallCheck
    consumerDir=$(mktemp -d)
    cp -R "$src/tests/cmake_consumer/." "$consumerDir/"
    cp -R "$src/tests/cache_headers" "$consumerDir/cache_headers"
    cmake -S "$consumerDir" -B "$consumerDir/build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER="$CXX" \
      -DCMAKE_PREFIX_PATH="$out" \
      -DCACHE_RESEARCH_MODE=installed \
      -DCACHE_RESEARCH_FORBIDDEN_SOURCE="$src" \
      -DCACHE_RESEARCH_FORBIDDEN_PREFIX="$consumerDir/unused-prefix"
    cmake --build "$consumerDir/build" --parallel "$NIX_BUILD_CORES"
    "$consumerDir/build/bin/Release/cache_consumer"
    runHook postInstallCheck
  '';

  meta = {
    description = "Header-only C++17 cache algorithms and multilevel cache systems";
    homepage = "https://github.com/timoko3/cacheResearch";
    platforms = lib.platforms.unix;
  };
}
