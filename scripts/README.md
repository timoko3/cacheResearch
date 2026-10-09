# C++ quality checks

Run from the repository root with Python 3, clang-format 18.1.3 and clang-tidy 18
on PATH. Formatting checks reject a different formatter version:

```sh
python3 scripts/code_quality.py format
python3 scripts/code_quality.py format-fix
cmake --preset debug
python3 scripts/code_quality.py tidy --build-dir build/debug
```

`format` is read-only and fails on formatting differences. `format-fix` applies
the same style. Editors should also use clang-format 18 and `--style=file`.
The root configuration explicitly specifies every option dumped by clang-format
18.1.3. Style decisions follow the original cache.h and cacheSystem.h: four spaces,
attached braces, inline short member functions, unindented namespace contents,
and indented switch labels. Existing differences between those headers are
normalized to one style; names are not changed. The line length is fixed at 100.

The benchmark directory inherits the root configuration, so editor and CI
formatting agree. New source files must be added to Git before these commands
will include them.

The checks use Git's tracked file list, excluding submodules, `third_party`,
and untracked build outputs. Formatting covers project sources and headers.
Clang-tidy analyzes project translation units and reports diagnostics in
project headers. Sources declared in CMake use `compile_commands.json`;
standalone tools use C++17 and the repository root include path.

Clang-tidy runs the Clang Static Analyzer and selected defect-oriented bugprone
checks. Naming, modernization, optional-residency contracts and exception
policies are not enforced. Enabled diagnostics fail CI. Compiler errors remain
fatal even when originating in dependencies.
