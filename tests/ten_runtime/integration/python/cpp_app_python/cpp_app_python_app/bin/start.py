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

# On Windows, prepare Python runtime dependencies
if sys.platform == "win32":
    # Create .pyd file from .dll for Python import
    dll_path = os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib', 'ten_runtime_python.dll')
    pyd_path = os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib', 'libten_runtime_python.pyd')

    if os.path.exists(dll_path) and not os.path.exists(pyd_path):
        print(f"Creating Python extension module: {pyd_path}")
        shutil.copy2(dll_path, pyd_path)

    # Copy dependency DLLs to python_addon_loader directory
    # Required for multiprocessing child processes on Windows.
    #
    # Root cause: LoadLibraryEx with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR flag
    # (core/src/ten_utils/lib/sys/win/module.c:23-25) ignores PATH and only
    # searches the loading DLL's directory for dependencies.
    #
    # See: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa
    # "If this value is used, the directory that contains the DLL is temporarily
    # added to the beginning of the list of directories that are searched for the
    # DLL's dependencies. Directories in the standard search path are not searched."
    addon_loader_lib_dir = os.path.join(app_root, 'ten_packages', 'addon_loader', 'python_addon_loader', 'lib')
    os.makedirs(addon_loader_lib_dir, exist_ok=True)

    dll_sources = [
        (os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime', 'lib'), ['ten_runtime.dll', 'ten_utils.dll']),
        (os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib'), ['ten_runtime_python.dll']),
        (os.path.dirname(sys.executable), ['python310.dll']),
    ]

    print(f"\n=== Copying dependency DLLs to python_addon_loader directory ===")
    for src_dir, dll_files in dll_sources:
        for dll_file in dll_files:
            src_path = os.path.join(src_dir, dll_file)
            dst_path = os.path.join(addon_loader_lib_dir, dll_file)

            if os.path.exists(src_path):
                if not os.path.exists(dst_path) or os.path.getmtime(src_path) > os.path.getmtime(dst_path):
                    try:
                        shutil.copy2(src_path, dst_path)
                        print(f"  [OK] Copied {dll_file} to addon_loader/lib")
                    except Exception as e:
                        print(f"  [ERROR] Failed to copy {dll_file}: {e}")
                else:
                    print(f"  [SKIP] {dll_file} already up-to-date")
            else:
                print(f"  [MISSING] {src_path}")
    print(f"===\n")

# Set environment variables and run the C++ application
env = os.environ.copy()

# Set PYTHONPATH - required for Python to find ten_runtime_python modules
pythonpath_parts = [
    os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib'),
    os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'interface')
]

if 'PYTHONPATH' in env:
    pythonpath_parts.append(env['PYTHONPATH'])

env['PYTHONPATH'] = os.pathsep.join(pythonpath_parts)

# On Windows, add DLL directories to PATH
# Required for main process (cpp_app_python_app.exe) to load DLLs at startup.
# Note: This is different from the DLL copy above:
# - PATH: For main EXE loading (uses standard LoadLibrary)
# - DLL copy: For python_addon_loader in child processes (uses LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR)
if sys.platform == "win32":
    dll_dirs = [
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime', 'lib'),
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib'),
        os.path.join(app_root, 'ten_packages', 'addon_loader', 'python_addon_loader', 'lib'),
    ]
    env['PATH'] = os.pathsep.join(dll_dirs) + os.pathsep + env.get('PATH', '')

# Run the C++ executable
if sys.platform == "win32":
    cpp_app_path = os.path.join(app_root, 'bin', 'cpp_app_python_app.exe')
else:
    cpp_app_path = os.path.join(app_root, 'bin', 'cpp_app_python_app')

sys.exit(subprocess.run([cpp_app_path], env=env).returncode)
