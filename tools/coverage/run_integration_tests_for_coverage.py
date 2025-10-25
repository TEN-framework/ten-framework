#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from typing import cast


def run(
    cmd: list[str], cwd: str | None = None, env: dict[str, str] | None = None
) -> int:
    print("$", " ".join(cmd))
    return subprocess.call(cmd, cwd=cwd, env=env)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-out", required=True)
    parser.add_argument("--is-clang", required=True, choices=["true", "false"])
    parser.add_argument("--done-file", required=True)
    return parser.parse_args()


def main() -> int:
    ns = parse_args()
    root_out: str = os.path.abspath(cast(str, ns.root_out))
    is_clang: bool = cast(str, ns.is_clang) == "true"
    done_file: str = os.path.abspath(cast(str, ns.done_file))

    # Setup coverage env for clang
    env: dict[str, str] = os.environ.copy()
    if is_clang:
        prof_dir: str = os.path.join(root_out, "cov")
        os.makedirs(prof_dir, exist_ok=True)
        env["LLVM_PROFILE_FILE"] = os.path.join(
            prof_dir, "profile-int-%p-%m.profraw"
        )

    # Make out/<os>/<cpu>/tests importable as a package root for implicit
    # namespace packages
    tests_root: str = os.path.join(root_out, "tests")
    env["PYTHONPATH"] = tests_root + (
        os.pathsep + env["PYTHONPATH"]
        if "PYTHONPATH" in env and env["PYTHONPATH"]
        else ""
    )

    # Run all integration tests containing test_case.py as modules (-m)
    ret: int = 0
    integ_root: str = os.path.join(tests_root, "ten_runtime", "integration")

    # Parse tgn args to get feature flags
    args_txt: str = os.path.join(root_out, "tgn_args.txt")
    flags: dict[str, str] = {}
    try:
        with open(args_txt, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    flags[k.strip()] = v.strip().strip('"')
    except Exception:
        pass

    # Heuristic: if numpy not available, skip ffmpeg_* tests which require numpy
    numpy_available: bool = False
    try:
        import importlib  # type: ignore

        importlib.import_module("numpy")
        numpy_available = True
    except Exception:
        numpy_available = False

    for root, _dirs, files in os.walk(integ_root):
        if "test_case.py" not in files:
            continue

        # Skip ffmpeg-related tests if numpy is not available
        _skip = (not numpy_available) and (
            "/ffmpeg_" in root or "/ffmpeg/" in root
        )
        if _skip:
            print(f"skip {root} due to missing numpy for ffmpeg tests")
            continue

        test_py: str = os.path.join(root, "test_case.py")
        rel: str = os.path.relpath(test_py, tests_root)
        module: str = rel[:-3].replace(os.sep, ".")  # strip .py
        cmd: list[str] = [sys.executable, "-m", module]
        ret |= run(cmd, env=env)

    # Done file for GN action outputs
    os.makedirs(os.path.dirname(done_file), exist_ok=True)
    with open(done_file, "w", encoding="utf-8") as f:
        f.write("ok\n")

    return ret


if __name__ == "__main__":
    sys.exit(main())
