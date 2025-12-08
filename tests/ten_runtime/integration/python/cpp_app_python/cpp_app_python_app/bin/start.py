#!/usr/bin/env python3
"""
Cross-platform start script for C++ app with Python extensions.

Compared to pure Python apps (default_app_python), this script needs extra
setup because Python is embedded in a C++ executable, which loses the automatic
environment configuration that python.exe provides.
"""

import os
import sys
import subprocess
import shutil

# Change to the project root directory
app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(app_root)

# On Windows, python extension module is a .pyd file, not a .dll file.
if sys.platform == "win32":
    # Create .pyd file from .dll for Python import
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

env = os.environ.copy()

# Find libpython using find_libpython.py and set TEN_PYTHON_LIB_PATH
# This is needed to specify the version of the embedded Python interpreter
find_libpython_script = os.path.join(
    app_root,
    "ten_packages",
    "system",
    "ten_runtime_python",
    "tools",
    "find_libpython.py",
)
if os.path.exists(find_libpython_script):
    try:
        result = subprocess.run(
            [sys.executable, find_libpython_script],
            capture_output=True,
            text=True,
            check=False,
        )
        libpython_path = result.stdout.strip()
        if libpython_path:
            env["TEN_PYTHON_LIB_PATH"] = libpython_path
    except Exception:
        pass  # Ignore errors if find_libpython.py fails

# On Windows, add DLL directories to PATH
# Required for main process (cpp_app_python_app.exe) to load ten_runtime.dll and ten_utils.dll
# at startup.(listed in its PE import table)
# This action is the same as the pure Cpp apps (tests/ten_runtime/integration/cpp/xxx/test_case.py)
if sys.platform == "win32":
    env["PATH"] = (
        os.path.join(app_root, "ten_packages", "system", "ten_runtime", "lib")
        + os.pathsep
        + env.get("PATH", "")
    )

# Run the C++ executable
if sys.platform == "win32":
    cpp_app_path = os.path.join(app_root, "bin", "cpp_app_python_app.exe")
else:
    cpp_app_path = os.path.join(app_root, "bin", "cpp_app_python_app")

sys.exit(subprocess.run([cpp_app_path], env=env).returncode)
