#!/usr/bin/env python3
"""
Windows-compatible start script for Go extension testing.

Note: AddressSanitizer (-sanitize flag) is not supported on Windows and has been omitted.
The -race flag functionality is preserved for race condition detection.
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    # Check if -race flag is passed
    race_flag = ""
    if "-race" in sys.argv:
        race_flag = "-race"
        print("Race detector enabled")

    # Note: -sanitize flag is not supported on Windows
    # AddressSanitizer (ASAN) is disabled on Windows builds
    if "-sanitize" in sys.argv:
        print("⚠️  Warning: AddressSanitizer is not supported on Windows")
        print("    The -sanitize flag will be ignored.")
        print()

    # Get the script directory and jump to extension root
    script_dir = Path(__file__).parent
    extension_root = script_dir.parent.parent
    os.chdir(extension_root)

    # Check if .ten/app directory exists
    if not Path(".ten/app").is_dir():
        print("The .ten/app directory does not exist. Please install it first.")
        sys.exit(1)

    # Get extension name from current directory
    extension_name = Path.cwd().name

    # Check if extension is installed in standalone mode
    extension_path = Path(f".ten/app/ten_packages/extension/{extension_name}")
    if not extension_path.is_dir():
        print(f"The .ten/app/ten_packages/extension/{extension_name} directory does not exist.")
        print("Please install with standalone mode.")
        sys.exit(1)

    # Jump to the extension directory
    os.chdir(extension_path)

    runtime_lib = ".ten/app/ten_packages/system/ten_runtime/lib"
    runtime_go_lib = ".ten/app/ten_packages/system/ten_runtime_go/lib"

    # -L helps the linker to find DLLs at compile time
    cgo_ldflags = f"-L{runtime_lib} -L{runtime_go_lib} -lten_utils -lten_runtime -lten_runtime_go"

    os.environ["CGO_LDFLAGS"] = cgo_ldflags
    os.environ["CGOCHECK"] = "2"

    # Windows doesn't support RPATH like Linux/Mac, so we need to add DLL directories to PATH
    # so the test binary can find them at runtime
    runtime_lib_abs = str(Path(runtime_lib).resolve())
    os.environ["PATH"] = f"{runtime_lib_abs};{os.environ.get('PATH', '')}"

    # Print current directory
    print(f"Current directory: {Path.cwd()}")

    # Set GOEXPERIMENT for cgocheck2 (Go 1.24+)
    os.environ["GOEXPERIMENT"] = "cgocheck2"
    print(f"GOEXPERIMENT={os.environ['GOEXPERIMENT']}")

    # Run go mod tidy
    print("Running go mod tidy...")
    subprocess.run(["go", "mod", "tidy"], check=True)

    # Compile test with optional -race flag
    test_binary = f".ten/app/bin/{extension_name}_test.exe"
    compile_cmd = ["go", "test", "-c"]
    if race_flag:
        compile_cmd.append(race_flag)
    compile_cmd.extend(["-gcflags=all=-d=checkptr=1", "./tests/...", "-o", test_binary])

    print(f"Compiling test: {' '.join(compile_cmd)}")
    subprocess.run(compile_cmd, check=True)

    # Set GOGC
    os.environ["GOGC"] = "1"

    # GODEBUG options to catch potential issues
    godebug_options = [
        "clobberfree=1",      # Overwrite freed memory to detect use-after-free
        "invalidptr=1",       # Crash on invalid pointers
        "gccheckmark=1",      # Verify GC mark phase correctness
    ]
    os.environ["GODEBUG"] = ",".join(godebug_options)
    print(f"GODEBUG={os.environ['GODEBUG']}")

    # Run the test
    print(f"\nRunning test: {test_binary}")
    result = subprocess.run([test_binary, "-test.v"])
    sys.exit(result.returncode)

if __name__ == "__main__":
    main()
