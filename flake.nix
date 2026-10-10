{
  description = "CacheResearch package and optional development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  inputs.general-functions = {
    url = "github:timoko3/generalFunctions/dc31aef3c60cb88428c6aa7b150edbc36028508e";
    flake = false;
  };

  outputs = { self, nixpkgs, general-functions, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      llvm = pkgs.llvmPackages_18;
      python = pkgs.python3.withPackages (packages: [ packages.pyyaml ]);
      gtest = pkgs.gtest.override { stdenv = llvm.stdenv; };
      package = llvm.stdenv.mkDerivation {
        pname = "cache-research";
        version = "0.1.0";
        src = self;
        nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
        nativeCheckInputs = [ gtest ];
        cmakeFlags = [
          "-DCACHE_RESEARCH_BUILD_APP=ON"
          "-DCACHE_RESEARCH_BUILD_BENCHMARKS=ON"
          "-DCACHE_RESEARCH_BUILD_TESTS=ON"
          "-DCACHE_RESEARCH_GENERAL_FUNCTIONS_SOURCE_DIR=${general-functions}"
          "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
        ];
        doCheck = true;
        checkPhase = ''
          runHook preCheck
          ctest --output-on-failure
          runHook postCheck
        '';
        meta.mainProgram = "cache_research";
      };
      qualityTools = map (tool: pkgs.writeShellScriptBin "${tool}-18" ''
        exec ${llvm.clang-tools}/bin/${tool} "$@"
      '') [ "clang-format" "clang-tidy" ];
    in
    {
      packages.${system}.default = package;
      checks.${system}.default = package;
      devShells.${system}.default = (pkgs.mkShell.override { stdenv = llvm.stdenv; }) {
        inputsFrom = [ package ];
        GENERAL_FUNCTIONS_SOURCE_DIR = "${general-functions}";
        packages = [
          pkgs.cmake
          pkgs.ninja
          pkgs.git
          python
        ] ++ qualityTools;

        shellHook = ''
          echo "CacheResearch environment: LLVM 18, CMake, Ninja and Python."
        '';
      };
    };
}
