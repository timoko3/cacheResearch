"""Run the same C++ quality checks locally and in CI (LLVM tools version 18)."""

import argparse
import json
import re
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}
FORMAT_VERSION = "18.1.3"


def check_format_version():
    output = subprocess.check_output(
        ["clang-format-18", "--version"], text=True, cwd=ROOT
    )
    match = re.search(r"\bversion\s+(\d+\.\d+\.\d+)\b", output)
    if match is None or match.group(1) != FORMAT_VERSION:
        raise SystemExit(
            f"Expected clang-format {FORMAT_VERSION}; got: {output.strip()}. "
            "Use the project's formatter version before checking or applying style."
        )


def project_files():
    # Gitlinks are not expanded: submodule sources and generated files are excluded.
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT)
    return [
        Path(name.decode("utf-8"))
        for name in tracked.split(b"\0")
        if name
        and Path(name.decode("utf-8")).suffix in SOURCE_SUFFIXES | HEADER_SUFFIXES
        and Path(name.decode("utf-8")).parts[0] not in {"third_party", "generalFunctions"}
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("check", choices=["format", "format-fix", "tidy"])
    parser.add_argument("--build-dir", default="build/debug")
    args = parser.parse_args()
    files = project_files()
    if not files:
        parser.error("No tracked project C/C++ files found")

    if args.check.startswith("format"):
        check_format_version()
        flags = ["-i"] if args.check == "format-fix" else ["--dry-run", "--Werror"]
        return subprocess.call(
            ["clang-format-18", "--style=file", *flags, *map(str, files)], cwd=ROOT
        )

    build_dir = (ROOT / args.build_dir).resolve()
    with (build_dir / "compile_commands.json").open(encoding="utf-8") as stream:
        database = json.load(stream)
    compiled = {
        (Path(entry["directory"]) / entry["file"]).resolve() for entry in database
    }
    failed = False
    for source in files:
        if source.suffix not in SOURCE_SUFFIXES:
            continue
        command = ["clang-tidy-18", str(source), "--config-file=.clang-tidy"]
        if (ROOT / source).resolve() in compiled:
            command.extend(["-p", str(build_dir)])
        else:
            # Standalone tools not declared in CMake still receive analysis.
            command.extend(["--", "-std=c++17", "-I", str(ROOT)])
        print(f"Analyzing {source}", flush=True)
        failed |= subprocess.call(command, cwd=ROOT) != 0
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
