#!/usr/bin/env python3
"""
Windows-compatible start script for Go extension testing.

Note: AddressSanitizer is not supported on Windows and has been omitted.
This is a simplified version for basic test execution.
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
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

    # Windows doesn't support RPATH like Linux/Mac, so we need to add DLL directories to PATH
    # so the test binary can find them at runtime
    runtime_lib_abs = str(Path(runtime_lib).resolve())
    os.environ["PATH"] = f"{runtime_lib_abs};{os.environ.get('PATH', '')}"

    # Print current directory
    print(f"Current directory: {Path.cwd()}")

    # Compile test - pass through any additional arguments
    test_binary = f".ten/app/bin/{extension_name}_test.exe"
    compile_cmd = ["go", "test", "-c", "./tests/...", "-o", test_binary]

    # Add any extra arguments passed to the script (excluding script name)
    if len(sys.argv) > 1:
        compile_cmd.extend(sys.argv[1:])

    print(f"Compiling test: {' '.join(compile_cmd)}")
    subprocess.run(compile_cmd, check=True)

    # Run the test
    print(f"\nRunning test: {test_binary}")
    result = subprocess.run([test_binary, "-test.v"])
    sys.exit(result.returncode)

if __name__ == "__main__":
    main()
