#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import shutil


def run(cmd: list[str], cwd: str | None = None, env: dict[str, str] | None = None) -> int:
	print("$", " ".join(cmd))
	return subprocess.call(cmd, cwd=cwd, env=env)


def run_logged(cmd: list[str], log_file: str, cwd: str | None = None, env: dict[str, str] | None = None, timeout_sec: int | None = None) -> int:
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
            f.write("\n[TIMEOUT] Process terminated after {} seconds.\n".format(timeout_sec))
            print("[TIMEOUT] Process terminated after {} seconds.".format(timeout_sec))
            return 124


def ensure_dir(p: str) -> None:
	os.makedirs(p, exist_ok=True)


def find_executable(root_out: str, name: str) -> str | None:
	cand = os.path.join(root_out, "tests", "standalone", name)
	if os.path.isfile(cand) and os.access(cand, os.X_OK):
		return cand
	# Windows suffix
	cand_exe = cand + ".exe"
	if os.path.isfile(cand_exe) and os.access(cand_exe, os.X_OK):
		return cand_exe
	return None


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--root-out", required=True)
	parser.add_argument("--report-out", required=True)
	parser.add_argument("--is-clang", required=True, choices=["true", "false"])
	args: argparse.Namespace = parser.parse_args()

	root_out = os.path.abspath(args.root_out)
	report_out = os.path.abspath(args.report_out)
	is_clang = args.is_clang == "true"

	ensure_dir(report_out)

    # Run tests to generate coverage artifacts
	unit = find_executable(root_out, "ten_runtime_unit_test")
	smoke = find_executable(root_out, "ten_runtime_smoke_test")

	env = os.environ.copy()

	# Set LLVM profile output pattern to a stable location
	if is_clang:
		prof_dir = os.path.join(root_out, "cov")
		ensure_dir(prof_dir)
		env["LLVM_PROFILE_FILE"] = os.path.join(prof_dir, "profile-%p-%m.profraw")

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
	skip_smoke = os.environ.get("TEN_COV_SKIP_SMOKE", "0").lower() in ("1", "true", "yes")
	smoke_filter = os.environ.get("TEN_COV_SMOKE_FILTER", "").strip()
	timeout_env = os.environ.get("TEN_COV_TEST_TIMEOUT_SEC", "")
	timeout_sec: int | None = int(timeout_env) if timeout_env.isdigit() else None

	rc = 0
	if unit:
		unit_cmd: list[str] = [unit]
		rc |= run_logged(unit_cmd, os.path.join(logs_dir, "unit.log"), cwd=os.path.dirname(unit), env=env, timeout_sec=timeout_sec)

	if smoke and not skip_smoke:
		smoke_cmd: list[str] = [smoke]
		if smoke_filter:
			smoke_cmd += [f"--gtest_filter={smoke_filter}"]
		rc |= run_logged(smoke_cmd, os.path.join(logs_dir, "smoke.log"), cwd=os.path.dirname(smoke), env=env, timeout_sec=timeout_sec)

	if rc != 0:
		print("Tests failed while collecting coverage", file=sys.stderr)
		# Continue to attempt generating report for debugging

	# Generate report
	if is_clang:
		# LLVM profile -> merge -> show html
		profraws = []
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
		objects = []
		# test executables
		if unit:
			objects.append(unit)
		if smoke:
			objects.append(smoke)
		# runtime shared libs (optional but improves attribution)
		rt_so = os.path.join(root_out, "libten_runtime.so")
		if os.path.exists(rt_so):
			objects.append(rt_so)
		# utils lib may also be instrumented; include if present
		utils_so = os.path.join(root_out, "libten_utils.so")
		if os.path.exists(utils_so):
			objects.append(utils_so)

		html = os.path.join(report_out, "index.html")
		unique_objects = list(dict.fromkeys(objects))
		obj_args = []
		for o in unique_objects:
			obj_args += ["-object", o]

		# Optional: export JSON for debugging (ignored if tool missing behavior changes)
		export_json = os.path.join(report_out, "coverage.json")
		export_cmd = [
			"llvm-cov",
			"export",
			"-instr-profile",
			merged,
			"-ignore-filename-regex",
			"(^|/)third_party/",
		] + obj_args
		with open(export_json, "w", encoding="utf-8") as f:
			print("$", " ".join(export_cmd))
			subprocess.call(export_cmd, stdout=f)

		cmd = [
			"llvm-cov",
			"show",
			"-instr-profile",
			merged,
			"-format=html",
			"-output-dir",
			report_out,
			"-ignore-filename-regex",
			"(^|/)third_party/",
		] + obj_args
		if run(cmd) != 0:
			print("llvm-cov show failed", file=sys.stderr)
			return 1

		# Generate plain text line-by-line annotated listing for quick grep
		line_txt = os.path.join(report_out, "line_coverage.txt")
		cmd_txt = [
			"llvm-cov",
			"show",
			"-instr-profile",
			merged,
			"-format=text",
			"-show-line-counts-or-regions",
			"-ignore-filename-regex",
			"(^|/)third_party/",
		] + obj_args
		with open(line_txt, "w", encoding="utf-8") as f:
			print("$", " ".join(cmd_txt))
			subprocess.call(cmd_txt, stdout=f)

		print(f"Coverage HTML: {html}")
		return 0
	else:
		# GCC gcov/lcov -> genhtml
		if not shutil.which("lcov") or not shutil.which("genhtml"):
			print("Missing lcov/genhtml in PATH", file=sys.stderr)
			return 1

		info = os.path.join(report_out, "coverage.info")
		# Capture from root_out
		if run(["lcov", "-c", "-d", root_out, "-o", info]) != 0:
			print("lcov capture failed", file=sys.stderr)
			return 1

		# Optionally filter out third_party; keep core/src/ten_runtime and tests/ten_runtime
		if run(["lcov", "-e", info, "*/core/src/ten_runtime/*", "*/tests/ten_runtime/*", "-o", info]) != 0:
			print("lcov filter failed", file=sys.stderr)
			return 1

		if run(["genhtml", info, "-o", report_out]) != 0:
			print("genhtml failed", file=sys.stderr)
			return 1

		print(f"Coverage HTML: {os.path.join(report_out, 'index.html')}")
		return 0


if __name__ == "__main__":
	sys.exit(main())
