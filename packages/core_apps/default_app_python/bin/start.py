#!/usr/bin/env python3
"""
Cross-platform start script for Python apps.
On Unix-like systems, prefer using bash start script for faster startup.
"""

import os
import sys
import subprocess
import shutil

# Change to the project root directory
app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(app_root)

# On Windows, create .pyd file from .dll for Python import
if sys.platform == "win32":
    dll_path = os.path.join(
        app_root,
        "ten_packages",
        "system",
        "ten_runtime_python",
        "lib",
        "ten_runtime_python.dll",
    )
    pyd_path = os.path.join(
        app_root,
        "ten_packages",
        "system",
        "ten_runtime_python",
        "lib",
        "libten_runtime_python.pyd",
    )

    if os.path.exists(dll_path) and not os.path.exists(pyd_path):
        print(f"Creating Python extension module: {pyd_path}")
        shutil.copy2(dll_path, pyd_path)

# Set environment variables and run the application
env = os.environ.copy()

# Set PYTHONPATH using the correct path separator for the platform
pythonpath_parts = [
    os.path.join(
        app_root, "ten_packages", "system", "ten_runtime_python", "lib"
    ),
    os.path.join(
        app_root, "ten_packages", "system", "ten_runtime_python", "interface"
    ),
]
env["PYTHONPATH"] = os.pathsep.join(pythonpath_parts)

sys.exit(subprocess.run([sys.executable, "main.py"], env=env).returncode)
