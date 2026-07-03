#!/usr/bin/env python3
"""
Cross-platform bootstrap script for Python apps on Windows.
On Unix-like systems, prefer using bash bootstrap script for faster startup.
"""

import os
import sys
import subprocess

# Change to the project root directory
app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(app_root)

# Resolve the dependencies of the Python app and generate the
# 'merged_requirements.txt' file.
deps_resolver = os.path.join(
    app_root,
    "ten_packages",
    "system",
    "ten_runtime_python",
    "tools",
    "deps_resolver.py",
)

result = subprocess.run([sys.executable, deps_resolver])
if result.returncode != 0:
    print("Error: Failed to resolve the dependencies of the Python app.")
    sys.exit(result.returncode)

merged_requirements = os.path.join(app_root, "merged_requirements.txt")
if os.path.exists(merged_requirements):
    print("The 'merged_requirements.txt' file is generated successfully.")
    # Pip install the dependencies in the 'merged_requirements.txt' file.
    result = subprocess.run(
        [sys.executable, "-m", "pip", "install", "-r", merged_requirements]
    )
    sys.exit(result.returncode)
else:
    print(
        "No 'merged_requirements.txt' file is generated, "
        "because there are no dependencies."
    )
    sys.exit(0)
