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


def load_env_file(app_dir):
    """Load .env file if it exists."""
    env_file = app_dir / ".env"
    if env_file.exists():
        print(
            f"[start.py] Loading environment from {env_file}", file=sys.stderr
        )
        try:
            with open(env_file, "r") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        if "=" in line:
                            key, value = line.split("=", 1)
                            os.environ[key.strip()] = value.strip()
                            print(
                                f"[start.py] Set {key.strip()}", file=sys.stderr
                            )
        except Exception as e:
            print(f"[start.py] Error loading .env file: {e}", file=sys.stderr)


def main():
    # Get the script directory and navigate to parent (tenapp root)
    script_dir = Path(__file__).parent
    app_dir = script_dir.parent

    # Load .env file first
    load_env_file(app_dir)

    # Change to app directory
    os.chdir(app_dir)

    # Set environment variables
    # Add both the interface directory and the parent directory for module imports
    pythonpath_parts = [
        str(app_dir / "ten_packages" / "system" / "ten_ai_base" / "interface"),
        str(
            app_dir
            / "ten_packages"
            / "system"
            / "ten_runtime_python"
            / "interface"
        ),
        str(app_dir / "ten_packages" / "system"),  # For ten_ai_base module
    ]
    pythonpath = os.pathsep.join(pythonpath_parts)
    if "PYTHONPATH" in os.environ:
        pythonpath = f"{pythonpath}{os.pathsep}{os.environ['PYTHONPATH']}"
    os.environ["PYTHONPATH"] = pythonpath
    print(
        f"[start.py] PYTHONPATH set to: {os.environ['PYTHONPATH']}",
        file=sys.stderr,
    )

    # Set library paths based on platform
    lib_paths = []
    lib_candidates = [
        app_dir
        / "ten_packages"
        / "system"
        / "ten_runtime"
        / "lib",  # Critical: ten_runtime.dll and ten_utils.dll
        app_dir / "ten_packages" / "system" / "agora_rtc_sdk" / "lib",
        app_dir / "ten_packages" / "extension" / "agora_rtm" / "lib",
        app_dir / "ten_packages" / "system" / "azure_speech_sdk" / "lib",
    ]

    # Only add paths that actually exist
    for lib_path in lib_candidates:
        if lib_path.exists():
            lib_paths.append(str(lib_path))
            print(
                f"[start.py] Adding library path: {lib_path}", file=sys.stderr
            )
        else:
            print(
                f"[start.py] Library path not found (skipping): {lib_path}",
                file=sys.stderr,
            )

    if platform.system() == "Windows":
        # On Windows, use PATH for DLLs
        path_var = os.environ.get("PATH", "")
        if lib_paths:
            os.environ["PATH"] = (
                f"{os.pathsep.join(lib_paths)}{os.pathsep}{path_var}"
            )
            print(
                f"[start.py] Updated PATH with library directories",
                file=sys.stderr,
            )
    else:
        # On Unix-like systems, use LD_LIBRARY_PATH
        ld_lib_path = os.environ.get("LD_LIBRARY_PATH", "")
        if lib_paths:
            os.environ["LD_LIBRARY_PATH"] = (
                f"{os.pathsep.join(lib_paths)}{os.pathsep}{ld_lib_path}"
            )
            print(
                f"[start.py] Updated LD_LIBRARY_PATH with library directories",
                file=sys.stderr,
            )

    # Set NODE_PATH
    node_path = str(
        app_dir / "ten_packages" / "system" / "ten_runtime_nodejs" / "lib"
    )
    if "NODE_PATH" in os.environ:
        node_path = f"{node_path}{os.pathsep}{os.environ['NODE_PATH']}"
    os.environ["NODE_PATH"] = node_path

    # Determine the main binary name
    main_binary = "main.exe" if platform.system() == "Windows" else "main"
    main_path = app_dir / "bin" / main_binary

    # Debug: Print environment info
    print(f"[start.py] App directory: {app_dir}", file=sys.stderr)
    print(f"[start.py] Main binary: {main_path}", file=sys.stderr)
    print(f"[start.py] Binary exists: {main_path.exists()}", file=sys.stderr)
    print(
        f"[start.py] PYTHONPATH: {os.environ.get('PYTHONPATH', 'NOT SET')}",
        file=sys.stderr,
    )
    print(
        f"[start.py] Executing: {main_path} {' '.join(sys.argv[1:])}",
        file=sys.stderr,
    )
    sys.stderr.flush()

    # Execute the main binary with all passed arguments
    try:
        # Use shell=False to avoid shell interpretation issues on Windows
        result = subprocess.run(
            [str(main_path)] + sys.argv[1:], capture_output=False, text=True
        )
        sys.exit(result.returncode)
    except FileNotFoundError:
        print(f"[start.py] Error: {main_path} not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"[start.py] Error: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc(file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
