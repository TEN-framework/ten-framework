#!/usr/bin/env python3
"""
Cross-platform start script for Go apps on Windows.
On Unix-like systems, prefer using bash start script for faster startup.
"""

import os
import sys
import subprocess

# Change to the project root directory
app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(app_root)

env = os.environ.copy()

main_exe = os.path.join(app_root, "bin", "main.exe")

if not os.path.exists(main_exe):
    print(f"Error: {main_exe} not found")
    sys.exit(1)

sys.exit(subprocess.run([main_exe], env=env).returncode)
