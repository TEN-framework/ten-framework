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

# Change to the extension root directory
script_dir = os.path.dirname(os.path.abspath(__file__))
extension_root = os.path.abspath(os.path.join(script_dir, "../.."))
os.chdir(extension_root)

# Install Python dependencies from requirements.txt
requirements_file = os.path.join(extension_root, "requirements.txt")
if os.path.exists(requirements_file):
    cmd = [sys.executable, "-m", "pip", "install", "-r", requirements_file]
    sys.exit(subprocess.call(cmd))
else:
    print(f"Warning: requirements.txt not found at {requirements_file}")
    sys.exit(0)
