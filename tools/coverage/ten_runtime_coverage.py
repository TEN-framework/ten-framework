#!/usr/bin/env python3
"""
Ten Runtime Coverage Report Generator

This script generates coverage reports for ten_runtime tests using either:
1. Clang toolchain (LLVM profdata/llvm-cov) - Currently used in CI
2. GCC toolchain (gcov/lcov/genhtml) - Available for local development

Current CI Configuration:
- Uses Clang toolchain (is_clang=true)
- Generates lcov format for Coveralls integration
- Includes file cleanup to reduce artifact size

GCC Support:
- Code is preserved for future compatibility
- Currently missing lcov format generation for Coveralls
- Can be used for local development and testing
"""
import argparse
import os
import subprocess
import sys
import shutil


def run(
    cmd: list[str], cwd: str | None = None, env: dict[str, str] | None = None
) -> int:
    print("$", " ".join(cmd))
    return subprocess.call(cmd, cwd=cwd, env=env)


def run_logged(
    cmd: list[str],
    log_file: str,
    cwd: str | None = None,
    env: dict[str, str] | None = None,
    timeout_sec: int | None = None,
) -> int:
    print("$", " ".join(cmd), f"> {log_file}")
    with open(log_file, "w", encoding="utf-8") as f:
        proc = subprocess.Popen(
            cmd,
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        try:
            assert proc.stdout is not None
            for line in proc.stdout:
                print(line, end="")
                f.write(line)
            return proc.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            try:
                proc.kill()
            except Exception:
                pass
            f.write(
                "\n[TIMEOUT] Process terminated after {} seconds.\n".format(
                    timeout_sec
                )
            )
            print(
                "[TIMEOUT] Process terminated after {} seconds.".format(
                    timeout_sec
                )
            )
            return 124


def ensure_dir(p: str) -> None:
    os.makedirs(p, exist_ok=True)


def find_executable(root_out: str, name: str) -> str | None:
    cand = os.path.join(root_out, "tests", "standalone", name)
    if os.path.isfile(cand) and os.access(cand, os.X_OK):
        return cand
    cand_exe = cand + ".exe"
    if os.path.isfile(cand_exe) and os.access(cand_exe, os.X_OK):
        return cand_exe
    return None


def free_tcp_port(port: int) -> None:
    try:
        subprocess.call(
            ["fuser", "-k", f"{port}/tcp"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except Exception:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-out", required=True)
    parser.add_argument("--report-out", required=True)
    parser.add_argument("--is-clang", required=True, choices=["true", "false"])
    parser.add_argument(
        "--generate-lcov",
        action="store_true",
        help="Generate lcov format file for Coveralls",
    )
    args: argparse.Namespace = parser.parse_args()

    root_out = os.path.abspath(args.root_out)
    report_out = os.path.abspath(args.report_out)
    is_clang = args.is_clang == "true"

    ensure_dir(report_out)

    # Run tests to generate coverage artifacts
    unit = find_executable(root_out, "ten_runtime_unit_test")
    smoke = find_executable(root_out, "ten_runtime_smoke_test")

    # Find integration test executables (C++ parts)
    integration_tests = []
    integration_dir = os.path.join(
        root_out, "tests", "ten_runtime", "integration", "cpp"
    )
    if os.path.exists(integration_dir):
        for item in os.listdir(integration_dir):
            item_path = os.path.join(integration_dir, item)
            if os.path.isdir(item_path):
                # Look for client executables in each integration test directory
                client_exe = os.path.join(item_path, f"{item}_app_client")
                if os.path.isfile(client_exe) and os.access(
                    client_exe, os.X_OK
                ):
                    integration_tests.append(client_exe)

    env = os.environ.copy()

    # Set LLVM profile output pattern to a stable location
    if is_clang:
        prof_dir = os.path.join(root_out, "cov")
        ensure_dir(prof_dir)
        env["LLVM_PROFILE_FILE"] = os.path.join(
            prof_dir, "profile-%p-%m.profraw"
        )

    # Clean previous data
    for ext in (".gcda", ".gcno", ".profraw", ".profdata", ".info"):
        for dirpath, _, filenames in os.walk(root_out):
            for fn in filenames:
                if fn.endswith(ext) or fn == "default.profraw":
                    try:
                        os.remove(os.path.join(dirpath, fn))
                    except Exception:
                        pass

    # Logging and controls
    logs_dir = os.path.join(report_out, "logs")
    ensure_dir(logs_dir)
    skip_smoke = os.environ.get("TEN_COV_SKIP_SMOKE", "0").lower() in (
        "1",
        "true",
        "yes",
    )
    smoke_filter = os.environ.get("TEN_COV_SMOKE_FILTER", "").strip()
    timeout_env = os.environ.get("TEN_COV_TEST_TIMEOUT_SEC", "")
    timeout_sec: int | None = (
        int(timeout_env) if timeout_env.isdigit() else None
    )

    rc = 0
    if unit:
        unit_cmd: list[str] = [unit]
        rc |= run_logged(
            unit_cmd,
            os.path.join(logs_dir, "unit.log"),
            cwd=os.path.dirname(unit),
            env=env,
            timeout_sec=timeout_sec,
        )

    if smoke and not skip_smoke:
        # Avoid CI port collision on 127.0.0.1:8001 used by smoke tests
        free_tcp_port(8001)
        smoke_cmd: list[str] = [smoke]
        if smoke_filter:
            smoke_cmd += [f"--gtest_filter={smoke_filter}"]
        rc |= run_logged(
            smoke_cmd,
            os.path.join(logs_dir, "smoke.log"),
            cwd=os.path.dirname(smoke),
            env=env,
            timeout_sec=timeout_sec,
        )

    if rc != 0:
        print("Tests failed while collecting coverage", file=sys.stderr)
        # Continue to attempt generating report for debugging

    # Generate report
    if is_clang:
        # LLVM profile -> merge -> show html
        profraws: list[str] = []
        for dirpath, _, filenames in os.walk(root_out):
            for fn in filenames:
                if fn.endswith(".profraw"):
                    profraws.append(os.path.join(dirpath, fn))

        if not shutil.which("llvm-profdata") or not shutil.which("llvm-cov"):
            print("Missing llvm-profdata/llvm-cov in PATH", file=sys.stderr)
            return 1

        merged = os.path.join(report_out, "coverage.profdata")
        cmd = ["llvm-profdata", "merge", "-sparse"] + profraws + ["-o", merged]
        if run(cmd) != 0:
            print("llvm-profdata merge failed", file=sys.stderr)
            return 1

        # Only include instrumented test binaries and runtime libraries
        objects: list[str] = []
        if unit:
            objects.append(unit)
        if smoke:
            objects.append(smoke)
        # Add integration test executables
        objects.extend(integration_tests)
        rt_so = os.path.join(root_out, "libten_runtime.so")
        if os.path.exists(rt_so):
            objects.append(rt_so)

        html = os.path.join(report_out, "index.html")
        unique_objects = list(dict.fromkeys(objects))
        obj_args: list[str] = []
        for o in unique_objects:
            obj_args += ["-object", o]

        # Export JSON (debug aid)
        export_json = os.path.join(report_out, "coverage.json")
        _ignore = "(^|/)third_party/|(^|/)tests/|(^|/)core/src/ten_utils/|(^|/)core/src/ten_manager/|(^|/)core/src/ten_rust/"
        export_cmd = [
            "llvm-cov",
            "export",
            "-instr-profile",
            merged,
            "-ignore-filename-regex",
            _ignore,
        ] + obj_args
        with open(export_json, "w", encoding="utf-8") as f:
            print("$", " ".join(export_cmd))
            subprocess.call(export_cmd, stdout=f)

        # HTML report
        cmd = [
            "llvm-cov",
            "show",
            "-instr-profile",
            merged,
            "-format=html",
            "-output-dir",
            report_out,
            "-ignore-filename-regex",
            _ignore,
        ] + obj_args
        if run(cmd) != 0:
            print("llvm-cov show failed", file=sys.stderr)
            return 1

        # Text line-by-line report
        line_txt = os.path.join(report_out, "line_coverage.txt")
        cmd_txt = [
            "llvm-cov",
            "show",
            "-instr-profile",
            merged,
            "-format=text",
            "-show-line-counts-or-regions",
            "-ignore-filename-regex",
            _ignore,
        ] + obj_args
        with open(line_txt, "w", encoding="utf-8") as f:
            print("$", " ".join(cmd_txt))
            subprocess.call(cmd_txt, stdout=f)

        # Generate lcov format for Coveralls if requested
        if args.generate_lcov:
            lcov_file = os.path.join(report_out, "coverage.lcov")
            lcov_cmd = [
                "llvm-cov",
                "export",
                "--format=lcov",
                "-instr-profile",
                merged,
                "-ignore-filename-regex",
                _ignore,
            ] + obj_args
            with open(lcov_file, "w", encoding="utf-8") as f:
                print("$", " ".join(lcov_cmd))
                subprocess.call(lcov_cmd, stdout=f)
            print(f"Coverage LCOV: {lcov_file}")

        print(f"Coverage HTML: {html}")

        # Clean up large files to reduce artifact size
        print("Cleaning up large files to reduce artifact size...")

        # Remove HTML coverage directory (contains source code copies)
        # NOTE: This removes the detailed HTML reports with source code
        # The main index.html still works but links to individual files will be
        # broken
        html_coverage_dir = os.path.join(report_out, "coverage")
        if os.path.exists(html_coverage_dir):
            shutil.rmtree(html_coverage_dir)
            print(f"Removed HTML coverage directory: {html_coverage_dir}")

        # Remove large log files (keep only summary)
        logs_dir = os.path.join(report_out, "logs")
        if os.path.exists(logs_dir):
            for log_file in os.listdir(logs_dir):
                log_path = os.path.join(logs_dir, log_file)
                if os.path.getsize(log_path) > 1024 * 1024:  # > 1MB
                    os.remove(log_path)
                    print(f"Removed large log file: {log_file}")

        # Remove large JSON file (keep lcov for Coveralls)
        json_file = os.path.join(report_out, "coverage.json")
        if os.path.exists(json_file):
            os.remove(json_file)
            print(f"Removed large JSON file: coverage.json")

        # Remove profdata file (not needed for final report)
        profdata_file = os.path.join(report_out, "coverage.profdata")
        if os.path.exists(profdata_file):
            os.remove(profdata_file)
            print(f"Removed profdata file: coverage.profdata")

        # Create a summary of what was removed for debugging
        summary_file = os.path.join(report_out, "cleanup_summary.txt")
        with open(summary_file, "w") as f:
            f.write("Coverage Report Cleanup Summary\n")
            f.write("===============================\n\n")
            f.write("Removed files to reduce artifact size:\n")
            f.write(
                "- coverage/ directory (HTML reports with source code copies)\n"
            )
            f.write("- Large log files (>1MB)\n")
            f.write("- coverage.json (raw coverage data)\n")
            f.write("- coverage.profdata (LLVM profdata)\n\n")
            f.write("Retained files for artifacts:\n")
            f.write("- index.html (main coverage summary)\n")
            f.write("- coverage.lcov (for Coveralls integration)\n")
            f.write("- line_coverage.txt (text format coverage report)\n")
            f.write("- style.css, control.js (HTML styling)\n")
            f.write("- Small log files (for debugging)\n")
            f.write("- cleanup_summary.txt (this file)\n")
        print(f"Created cleanup summary: {summary_file}")

        return 0
    else:
        # GCC gcov/lcov -> genhtml
        # NOTE: Currently only Clang toolchain is used in CI (is_clang=true)
        # This GCC path is kept for future compatibility and local development
        if not shutil.which("lcov") or not shutil.which("genhtml"):
            print("Missing lcov/genhtml in PATH", file=sys.stderr)
            return 1

        info = os.path.join(report_out, "coverage.info")
        if run(["lcov", "-c", "-d", root_out, "-o", info]) != 0:
            print("lcov capture failed", file=sys.stderr)
            return 1

        if (
            run(
                [
                    "lcov",
                    "-e",
                    info,
                    "*/core/src/ten_runtime/*",
                    "*/tests/ten_runtime/*",
                    "-o",
                    info,
                ]
            )
            != 0
        ):
            print("lcov filter failed", file=sys.stderr)
            return 1

        if run(["genhtml", info, "-o", report_out]) != 0:
            print("genhtml failed", file=sys.stderr)
            return 1

        print(f"Coverage HTML: {os.path.join(report_out, 'index.html')}")

        # TODO: Add lcov format generation for GCC toolchain
        # Currently GCC path doesn't generate coverage.lcov for Coveralls
        # This would require additional lcov commands to create lcov format

        # Clean up large files to reduce artifact size
        print("Cleaning up large files to reduce artifact size...")

        # Remove HTML coverage directory (contains source code copies)
        html_coverage_dir = os.path.join(report_out, "coverage")
        if os.path.exists(html_coverage_dir):
            shutil.rmtree(html_coverage_dir)
            print(f"Removed HTML coverage directory: {html_coverage_dir}")

        # Remove large log files (keep only summary)
        logs_dir = os.path.join(report_out, "logs")
        if os.path.exists(logs_dir):
            for log_file in os.listdir(logs_dir):
                log_path = os.path.join(logs_dir, log_file)
                if os.path.getsize(log_path) > 1024 * 1024:  # > 1MB
                    os.remove(log_path)
                    print(f"Removed large log file: {log_file}")

        # Remove large info file (keep lcov for Coveralls)
        info_file = os.path.join(report_out, "coverage.info")
        if os.path.exists(info_file):
            os.remove(info_file)
            print(f"Removed large info file: coverage.info")

        # Create a summary of what was removed for debugging
        summary_file = os.path.join(report_out, "cleanup_summary.txt")
        with open(summary_file, "w") as f:
            f.write("Coverage Report Cleanup Summary (GCC)\n")
            f.write("====================================\n\n")
            f.write("Removed files to reduce artifact size:\n")
            f.write(
                "- coverage/ directory (HTML reports with source code copies)\n"
            )
            f.write("- Large log files (>1MB)\n")
            f.write("- coverage.info (raw coverage data)\n\n")
            f.write("Retained files for artifacts:\n")
            f.write("- index.html (main coverage summary)\n")
            f.write("- line_coverage.txt (text format coverage report)\n")
            f.write("- style.css, control.js (HTML styling)\n")
            f.write("- Small log files (for debugging)\n")
            f.write("- cleanup_summary.txt (this file)\n")
        print(f"Created cleanup summary: {summary_file}")

        return 0


if __name__ == "__main__":
    sys.exit(main())
