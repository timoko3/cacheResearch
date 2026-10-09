# C++ quality checks

Run from the repository root with Python 3, clang-format 18.1.3 and clang-tidy 18
on PATH. Formatting checks reject a different formatter version:

```sh
python3 scripts/code_quality.py format
python3 scripts/code_quality.py format-fix
cmake --preset debug
python3 scripts/code_quality.py tidy --build-dir build/debug
```

To fix both naming and formatting, install PyYAML once and run:

```sh
python3 -m pip install PyYAML
cmake --preset debug
python3 scripts/code_quality.py style-fix
```

`style-fix` collects naming fixes from all translation units before applying
them to tracked project files, including shared headers. It then formats the
files and checks naming again. Conflicting fixes fail before writing; dependencies
are not edited. Only `readability-identifier-naming` fixes are applied.
Documentation and non-C++ references need manual updates after API renames.
PyYAML is required only for this command; normal CI checks stay unchanged.

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

Clang-tidy runs the Clang Static Analyzer, selected defect-oriented bugprone
checks and naming checks matching [the project code style](../CODE_STYLE.md).
Modernization, optional-residency contracts and exception policies are not
enforced. Enabled diagnostics fail CI. Compiler errors remain
fatal even when originating in dependencies.
