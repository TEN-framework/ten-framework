#!/usr/bin/env python3
"""
Cross-platform Python dependency installer for tenapp.
Works on Windows, Linux, and macOS.
"""

import io
import os
import sys
import subprocess
import platform
from pathlib import Path

# To solve Error: 'gbk' codec can't encode character '\u2713' in MinGW/Windows
# CJK environment. \u2713: ✓ (check sign)
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

def run_command(cmd, description):
    """Run a command and return success status."""
    print(f"Installing {description}...")
    try:
        result = subprocess.run(cmd, shell=True, check=False)
        if result.returncode == 0:
            print(f"✓ Successfully installed {description}")
            return True
        else:
            print(f"✗ Failed to install {description} (attempt failed)")
            return False
    except Exception as e:
        print(f"✗ Error installing {description}: {e}")
        return False


def install_with_retry(cmd, description, max_retries=3):
    """Try to install with retries."""
    for attempt in range(1, max_retries + 1):
        print(f"Attempt {attempt} of {max_retries}...")
        if run_command(cmd, description):
            return True
        if attempt < max_retries:
            print(f"Retrying in 2 seconds...")
            import time
            time.sleep(2)

    print(f"✗ Failed to install {description} after {max_retries} attempts")
    return False


def main():
    # Get the app directory
    script_dir = Path(__file__).parent
    app_dir = script_dir.parent
    os.chdir(app_dir)

    print(f"App root directory: {app_dir}")
    print(f"Platform: {platform.system()}")

    # Check if manifest.json exists
    if not (app_dir / "manifest.json").exists():
        print("Error: manifest.json file not found")
        sys.exit(1)

    print("Starting Python dependencies installation...")

    # Determine pip command
    pip_cmd = "uv pip install --system"

    # Try to install server requirements
    server_req = app_dir.parent / "server" / "requirements.txt"
    if server_req.exists():
        install_with_retry(f'{pip_cmd} -r "{server_req}"', "server requirements")
    else:
        print(f"No requirements.txt found in server directory")

    # Install extension requirements
    extension_dir = app_dir / "ten_packages" / "extension"
    if extension_dir.exists():
        print("Traversing ten_packages/extension directory...")
        for ext_path in extension_dir.iterdir():
            if ext_path.is_dir():
                req_file = ext_path / "requirements.txt"
                if req_file.exists():
                    # Continue on failure instead of stopping
                    install_with_retry(f'{pip_cmd} -r "{req_file}"', f"extension {ext_path.name}")
    else:
        print("ten_packages/extension directory not found")

    # Install system requirements
    system_dir = app_dir / "ten_packages" / "system"
    if system_dir.exists():
        print("Traversing ten_packages/system directory...")
        for sys_path in system_dir.iterdir():
            if sys_path.is_dir():
                req_file = sys_path / "requirements.txt"
                if req_file.exists():
                    # Continue on failure instead of stopping
                    install_with_retry(f'{pip_cmd} -r "{req_file}"', f"system {sys_path.name}")
    else:
        print("ten_packages/system directory not found")

    print("Python dependencies installation completed!")


if __name__ == "__main__":
    main()
