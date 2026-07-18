#!/usr/bin/env python3
"""
Cross-platform start script for Node.js apps on Windows.
On Unix-like systems, prefer using bash start script for faster startup.
"""

import os
import sys
import subprocess

# Change to the project root directory
app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(app_root)

env = os.environ.copy()

# Set NODE_PATH to include ten_runtime_nodejs lib
ten_runtime_nodejs_lib = os.path.join(
    app_root, "ten_packages", "system", "ten_runtime_nodejs", "lib"
)
if "NODE_PATH" in env:
    env["NODE_PATH"] = ten_runtime_nodejs_lib + os.pathsep + env["NODE_PATH"]
else:
    env["NODE_PATH"] = ten_runtime_nodejs_lib

# Add DLL search paths to PATH for Windows
# ten_runtime_nodejs.node depends on libnode.dll and ten_runtime.dll
ten_runtime_lib = os.path.join(
    app_root, "ten_packages", "system", "ten_runtime", "lib"
)
dll_paths = [ten_runtime_nodejs_lib, ten_runtime_lib]
env["PATH"] = os.pathsep.join(dll_paths) + os.pathsep + env.get("PATH", "")

# Run node with the start script
start_js = os.path.join(app_root, "build", "start.js")

if not os.path.exists(start_js):
    print(f"Error: {start_js} not found")
    sys.exit(1)

cmd = ["node", "--expose-gc", start_js]
sys.exit(subprocess.run(cmd, env=env).returncode)
