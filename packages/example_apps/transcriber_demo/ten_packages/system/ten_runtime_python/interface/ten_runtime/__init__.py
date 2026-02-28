#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

# ==============================================================================
# Windows DLL Search Path Configuration
# ==============================================================================
#
# Why is this needed?
#
# 1. Python 3.8+ on Windows no longer searches PATH for DLLs to prevent
#    DLL hijacking attacks.
#    Reference: https://docs.python.org/3/whatsnew/3.8.html#bpo-36085-whatsnew
#    : "PATH and the current working directory are no longer used, ...,  check for
#       add_dll_directory()"
#
# 2. libten_runtime_python.pyd:
#
#    - Python code uses libten_runtime_python.pyd (C extension) to access TEN
#    framework features through its C API. So in a Python app, this file is
#    executed when users run "from ten_runtime import ...".
#
#    - libten_runtime_python.pyd will be imported when any module in this package
#    (e.g., addon.py, app.py) executes "from libten_runtime_python import ...".
#
#    - libten_runtime_python.pyd depends on ten_runtime.dll and ten_utils.dll
#    (listed in its PE import table).
#
# ==============================================================================

import sys
import os

if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    # Get the directory of this __init__.py file
    _current_dir = os.path.dirname(os.path.abspath(__file__))

    # Navigate to ten_packages/system/ten_runtime_python/lib
    # Path: .../ten_packages/system/ten_runtime_python/interface/ten_runtime/__init__.py
    #    -> .../ten_packages/system/ten_runtime_python/lib
    # In order to find libten_runtime_python.pyd
    _lib_dir = os.path.join(
        os.path.dirname(os.path.dirname(_current_dir)), "lib"
    )

    # Also need ten_runtime/lib for dependent DLLs
    # Path: .../ten_packages/system/ten_runtime_python
    #    -> .../ten_packages/system/ten_runtime/lib
    # In order to find ten_runtime.dll, ten_utils.dll
    # which is required by libten_runtime_python.pyd
    # (written in its PE import table)
    _runtime_lib_dir = os.path.join(
        os.path.dirname(os.path.dirname(_current_dir)),
        "..",
        "ten_runtime",
        "lib",
    )

    # Collect directories to add to DLL search path
    _dll_dirs = [_lib_dir, _runtime_lib_dir]

    # Add MinGW bin directories from PATH to support MinGW runtime DLLs
    # This is needed because:
    # 1. ten_runtime.dll and ten_utils.dll both depend on libgcc_s_seh-1.dll
    # 2. libgcc_s_seh-1.dll depend on libwinpthread-1.dll
    # 3. Python 3.8+ doesn't use PATH for DLL loading by default
    _path_env = os.environ.get("PATH", "")
    if _path_env:
        for _path_dir in _path_env.split(os.pathsep):
            _path_dir = _path_dir.strip()
            if not _path_dir:
                continue

            # Check if this is a MinGW bin directory by examining:
            # 1. Path contains "mingw" (case-insensitive)
            # 2. Path ends with "bin"
            # 3. Directory contains GCC compiler or other MinGW tools
            _path_dir_lower = _path_dir.lower()
            _is_mingw_candidate = (
                "mingw" in _path_dir_lower
                and _path_dir_lower.endswith("bin")
            )

            if _is_mingw_candidate and os.path.isdir(_path_dir):
                # Verify it's a real MinGW directory by checking for compiler
                # or common MinGW tools
                _mingw_tools = [
                    "gcc.exe",
                    "g++.exe",
                    "mingw32-make.exe",
                    "ld.exe",
                ]
                for _tool in _mingw_tools:
                    _tool_path = os.path.join(_path_dir, _tool)
                    if os.path.isfile(_tool_path):
                        _dll_dirs.append(_path_dir)
                        break

    # Add all collected directories to DLL search path
    for _dir in _dll_dirs:
        _abs_dir = os.path.abspath(_dir)
        if os.path.isdir(_abs_dir):
            try:
                os.add_dll_directory(_abs_dir)
            except (OSError, AttributeError):
                # Silently ignore errors (e.g., directory already added, or
                # add_dll_directory not available on older Python/Windows versions)
                pass

# ==============================================================================
# Shared Library Search Path Configuration
# ==============================================================================
#
# We need to add the lib directory to sys.path so that Python can find the
# native extension module (e.g., libten_runtime_python.so/.pyd) when importing.
# ==============================================================================

# Get the directory of this __init__.py file
_current_dir = os.path.dirname(os.path.abspath(__file__))

# Navigate to ten_packages/system/ten_runtime_python/lib
# Path: .../ten_packages/system/ten_runtime_python/interface/ten_runtime/__init__.py
#    -> .../ten_packages/system/ten_runtime_python/lib
# In order to find libten_runtime_python.so
_lib_dir = os.path.join(os.path.dirname(os.path.dirname(_current_dir)), "lib")

# Add lib directory to sys.path so Python can find .so files
_abs_lib_dir = os.path.abspath(_lib_dir)
if os.path.isdir(_abs_lib_dir) and _abs_lib_dir not in sys.path:
    sys.path.insert(0, _abs_lib_dir)

from .addon import Addon
from .addon_manager import (
    register_addon_as_extension,
    _AddonManager,  # pyright: ignore[reportPrivateUsage]
)
from .app import App
from .extension import Extension
from .async_extension import AsyncExtension
from .async_ten_env import AsyncTenEnv
from .send_options import SendOptions
from .ten_env import TenEnv
from .log_level import LogLevel
from .log_option import LogOption, DefaultLogOption
from .error import TenError, TenErrorCode
from .value import Value, ValueType
from .test import ExtensionTester, TenEnvTester
from .async_test import AsyncExtensionTester, AsyncTenEnvTester
from .loc import Loc
from .cmd import Cmd
from .cmd_result import CmdResult, StatusCode
from .start_graph_cmd import StartGraphCmd
from .stop_graph_cmd import StopGraphCmd
from .trigger_life_cycle_cmd import TriggerLifeCycleCmd
from .data import Data
from .video_frame import VideoFrame, PixelFmt
from .audio_frame import AudioFrame, AudioFrameDataFmt

# Specify what should be imported when a user imports * from the
# ten_runtime_python package.
__all__ = [
    "Addon",
    "_AddonManager",
    "register_addon_as_extension",
    "App",
    "Extension",
    "AsyncExtension",
    "TenEnv",
    "TenErrorCode",
    "AsyncTenEnv",
    "SendOptions",
    "Cmd",
    "StatusCode",
    "StartGraphCmd",
    "StopGraphCmd",
    "TriggerLifeCycleCmd",
    "VideoFrame",
    "AudioFrame",
    "Data",
    "CmdResult",
    "PixelFmt",
    "AudioFrameDataFmt",
    "LogLevel",
    "LogOption",
    "DefaultLogOption",
    "ExtensionTester",
    "TenEnvTester",
    "TenError",
    "Value",
    "ValueType",
    "AsyncExtensionTester",
    "AsyncTenEnvTester",
    "Loc",
]
