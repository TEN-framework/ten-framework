#!/usr/bin/env python3
"""
Bootstrap script for Python apps (cross-platform)
Resolve the dependencies of the Python app and generate the 'merged_requirements.txt' file.
"""

import os
import sys
import subprocess
from pathlib import Path


def main():
    # Change to the app root directory (parent of bin/)
    script_dir = Path(__file__).parent
    app_root = script_dir.parent
    os.chdir(app_root)

    print(f"Working directory: {os.getcwd()}")

    # Path to deps_resolver.py
    deps_resolver = (
        app_root
        / "ten_packages"
        / "system"
        / "ten_runtime_python"
        / "tools"
        / "deps_resolver.py"
    )

    if not deps_resolver.exists():
        print(f"Error: deps_resolver.py not found at {deps_resolver}")
        return 1

    # Run deps_resolver.py
    print("Resolving dependencies...")
    try:
        result = subprocess.run(
            [sys.executable, str(deps_resolver)],
            check=True,
            capture_output=False,
        )
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to resolve the dependencies of the Python app.")
        return e.returncode

    # Check if merged_requirements.txt exists
    merged_requirements = app_root / "merged_requirements.txt"

    if merged_requirements.exists():
        print("The 'merged_requirements.txt' file is generated successfully.")
        print("Installing dependencies...")

        try:
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "pip",
                    "install",
                    "-r",
                    str(merged_requirements),
                ],
                check=True,
            )
            print("Dependencies installed successfully.")
        except subprocess.CalledProcessError as e:
            print(f"Error: Failed to install dependencies.")
            return e.returncode
    else:
        print(
            "No 'merged_requirements.txt' file is generated, because there are no dependencies."
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
