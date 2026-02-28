#!/usr/bin/env python3
"""
Cross-platform startup script for tenapp.
Sets up environment variables and executes the main binary.
"""

import os
import sys
import subprocess
import platform
from pathlib import Path

def main():
    # Get the script directory and navigate to parent (tenapp root)
    script_dir = Path(__file__).parent
    app_dir = script_dir.parent

    # Change to app directory
    os.chdir(app_dir)

    # Set environment variables
    pythonpath = str(app_dir / "ten_packages" / "system" / "ten_ai_base" / "interface")
    if "PYTHONPATH" in os.environ:
        pythonpath = f"{pythonpath}{os.pathsep}{os.environ['PYTHONPATH']}"
    os.environ["PYTHONPATH"] = pythonpath

    # Set library paths based on platform
    lib_paths = [
        str(app_dir / "ten_packages" / "system" / "agora_rtc_sdk" / "lib"),
        str(app_dir / "ten_packages" / "extension" / "agora_rtm" / "lib"),
        str(app_dir / "ten_packages" / "system" / "azure_speech_sdk" / "lib"),
    ]

    if platform.system() == "Windows":
        # On Windows, use PATH for DLLs
        path_var = os.environ.get("PATH", "")
        os.environ["PATH"] = f"{os.pathsep.join(lib_paths)}{os.pathsep}{path_var}"
    else:
        # On Unix-like systems, use LD_LIBRARY_PATH
        ld_lib_path = os.environ.get("LD_LIBRARY_PATH", "")
        os.environ["LD_LIBRARY_PATH"] = f"{os.pathsep.join(lib_paths)}{os.pathsep}{ld_lib_path}"

    # Set NODE_PATH
    node_path = str(app_dir / "ten_packages" / "system" / "ten_runtime_nodejs" / "lib")
    if "NODE_PATH" in os.environ:
        node_path = f"{node_path}{os.pathsep}{os.environ['NODE_PATH']}"
    os.environ["NODE_PATH"] = node_path

    # Determine the main binary name
    main_binary = "main.exe" if platform.system() == "Windows" else "main"
    main_path = app_dir / "bin" / main_binary

    # Execute the main binary with all passed arguments
    try:
        result = subprocess.run([str(main_path)] + sys.argv[1:])
        sys.exit(result.returncode)
    except FileNotFoundError:
        print(f"Error: {main_path} not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
