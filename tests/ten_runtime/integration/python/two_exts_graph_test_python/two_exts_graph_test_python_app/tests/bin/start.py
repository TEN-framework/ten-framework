#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import subprocess
import site
import shutil

# Change to the extension root directory
script_dir = os.path.dirname(os.path.abspath(__file__))
app_root = os.path.abspath(os.path.join(script_dir, "../.."))
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

# Set PYTHONPATH
pythonpath_parts = [
    "ten_packages/system/ten_runtime_python/lib",
    "ten_packages/system/ten_runtime_python/interface",
    "ten_packages/system/pytest_ten",
]

# If running in a virtual environment, add its site-packages to PYTHONPATH
# This allows pytest to find packages installed in the venv
if hasattr(sys, "real_prefix") or (
    hasattr(sys, "base_prefix") and sys.base_prefix != sys.prefix
):
    # We're in a virtual environment
    # Get the venv's site-packages directory
    venv_site_packages = site.getsitepackages()
    if venv_site_packages:
        pythonpath_parts.extend(venv_site_packages)

pythonpath = os.pathsep.join(
    [
        os.path.join(app_root, p) if not os.path.isabs(p) else p
        for p in pythonpath_parts
    ]
)
env = os.environ.copy()
if "PYTHONPATH" in env:
    env["PYTHONPATH"] = pythonpath + os.pathsep + env["PYTHONPATH"]
else:
    env["PYTHONPATH"] = pythonpath

# Run pytest with all command line arguments
cmd = [sys.executable, "-m", "pytest", "-s", "tests/"] + sys.argv[1:]
sys.exit(subprocess.call(cmd, env=env))
