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

    # Copy dependency DLLs for multiprocessing child processes on Windows.
    #
    # CRITICAL for multiprocessing: Child processes need DLLs in multiple locations:
    # 1. ten_runtime_python/lib/ - for libten_runtime_python.pyd import in child process
    # 2. python_addon_loader/lib/ - for python_addon_loader.dll's dependencies ONLY
    #
    # Root cause: LoadLibraryEx with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR flag
    # (core/src/ten_utils/lib/sys/win/module.c) ignores PATH and only searches
    # the loading DLL's directory for dependencies.
    #
    # Python 3.8+ .pyd import also ignores PATH and only searches:
    # - The .pyd file's directory
    # - Directories added via os.add_dll_directory()
    # - System directories

    # Define all target directories that need dependency DLLs
    target_dirs = [
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib'),
        os.path.join(app_root, 'ten_packages', 'addon_loader', 'python_addon_loader', 'lib'),
    ]

    # Source directories containing dependency DLLs
    # NOTE: Do NOT include python_addon_loader's parent directory, as that would
    # copy python_addon_loader.dll itself into lib/, causing load failures
    dll_source_dirs = [
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime', 'lib'),
        os.path.join(app_root, 'ten_packages', 'system', 'ten_runtime_python', 'lib'),
        os.path.dirname(sys.executable),  # Python DLLs
    ]

    print(f"\n=== Copying dependency DLLs for multiprocessing ===")
    for target_dir in target_dirs:
        os.makedirs(target_dir, exist_ok=True)
        print(f"Target: {os.path.basename(os.path.dirname(target_dir))}/{os.path.basename(target_dir)}")

        for src_dir in dll_source_dirs:
            if os.path.exists(src_dir):
                for filename in os.listdir(src_dir):
                    if filename.lower().endswith('.dll'):
                        # Skip copying python_addon_loader.dll to avoid path confusion
                        if filename == 'python_addon_loader.dll':
                            continue

                        src_path = os.path.join(src_dir, filename)
                        dst_path = os.path.join(target_dir, filename)

                        if not os.path.exists(dst_path) or os.path.getmtime(src_path) > os.path.getmtime(dst_path):
                            try:
                                shutil.copy2(src_path, dst_path)
                                print(f"  ✓ {filename}")
                            except Exception as e:
                                print(f"  ✗ {filename}: {e}")
        print()
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
    cpp_app_path = os.path.join(app_root, 'bin', 'cpp_app_multi_process_python_app.exe')
else:
    cpp_app_path = os.path.join(app_root, 'bin', 'cpp_app_multi_process_python_app')

sys.exit(subprocess.run([cpp_app_path], env=env).returncode)
