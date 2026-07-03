#!/usr/bin/env python3
"""
Cross-platform test start script for Node.js extensions on Windows.
On Unix-like systems, prefer using bash start script for faster startup.
"""

import os
import sys
import subprocess

# Change to the tests directory (parent of bin)
tests_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(tests_root)

env = os.environ.copy()

# npm install
print("Running npm install...")
result = subprocess.run(["npm", "install"], env=env, shell=True)
if result.returncode != 0:
    print("npm install failed")
    sys.exit(result.returncode)

# npm run build
print("Running npm run build...")
result = subprocess.run(["npm", "run", "build"], env=env, shell=True)
if result.returncode != 0:
    print("npm run build failed")
    sys.exit(result.returncode)

# Set NODE_PATH to include ten_runtime_nodejs lib
ten_runtime_nodejs_lib = os.path.join(
    tests_root, "..", ".ten", "app", "ten_packages", "system",
    "ten_runtime_nodejs", "lib"
)
ten_runtime_nodejs_lib = os.path.normpath(ten_runtime_nodejs_lib)
if "NODE_PATH" in env:
    env["NODE_PATH"] = ten_runtime_nodejs_lib + os.pathsep + env["NODE_PATH"]
else:
    env["NODE_PATH"] = ten_runtime_nodejs_lib

# Add DLL search paths to PATH for Windows
ten_runtime_lib = os.path.join(
    tests_root, "..", ".ten", "app", "ten_packages", "system",
    "ten_runtime", "lib"
)
ten_runtime_lib = os.path.normpath(ten_runtime_lib)
dll_paths = [ten_runtime_nodejs_lib, ten_runtime_lib]
env["PATH"] = os.pathsep.join(dll_paths) + os.pathsep + env.get("PATH", "")

# Run test directly with node (matching the bash script behavior)
print("Running node --expose-gc build/index.js...")
result = subprocess.run(["node", "--expose-gc", "build/index.js"], env=env)
sys.exit(result.returncode)
