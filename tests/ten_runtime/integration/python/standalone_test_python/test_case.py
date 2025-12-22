"""
Test standalone_test_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config


def test_standalone_test_python():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    extension_root_path = os.path.join(base_path, "default_extension_python")

    # Step 1:
    #
    # Standalone testing involves the use of ten_runtime_python, so use tman
    # install to install the ten_runtime_python system package.
    tman_install_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "install",
        "--standalone",
    ]

    tman_install_process = subprocess.Popen(
        tman_install_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tman_install_process.wait()
    return_code = tman_install_process.returncode
    if return_code != 0:
        assert False, "Failed to install package."

    # Step 2:
    #
    # Use uv to sync dependencies and run pytest.

    tests_dir = os.path.join(extension_root_path, "tests")

    # Run uv sync --all-packages to install dependencies.
    uv_sync_cmd = [
        "uv",
        "sync",
        "--all-packages",
    ]

    uv_sync_process = subprocess.Popen(
        uv_sync_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=tests_dir,
    )
    uv_sync_process.wait()
    return_code = uv_sync_process.returncode
    if return_code != 0:
        assert False, "Failed to sync dependencies with uv."

    # Step 3:
    #
    # Set the required environment variables for the test.

    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    # Find libpython path using the tool in ten_runtime_python package
    find_libpython_script = os.path.join(
        extension_root_path,
        ".ten/app/ten_packages/system/ten_runtime_python/tools/find_libpython.py",
    )
    if os.path.exists(find_libpython_script):
        get_lib_path_cmd = [
            "uv",
            "run",
            "python",
            find_libpython_script,
        ]
        try:
            lib_path = (
                subprocess.check_output(
                    get_lib_path_cmd,
                    cwd=tests_dir,
                    env=my_env,
                )
                .decode("utf-8")
                .strip()
            )

            if lib_path:
                print(f"Setting TEN_PYTHON_LIB_PATH to {lib_path}")
                my_env["TEN_PYTHON_LIB_PATH"] = lib_path
        except subprocess.CalledProcessError as e:
            print(f"Failed to find python lib path: {e}")

    if sys.platform == "linux":
        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                extension_root_path,
                (".ten/app/ten_packages/system/ten_runtime/lib/libasan.so"),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path
    elif sys.platform == "darwin":
        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                extension_root_path,
                (
                    ".ten/app/ten_packages/system/ten_runtime/lib/"
                    "libclang_rt.asan_osx_dynamic.dylib"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["DYLD_INSERT_LIBRARIES"] = libasan_path

    # Step 4:
    #
    # Run the test using uv run pytest.
    uv_run_pytest_cmd = [
        "uv",
        "run",
        "pytest",
        "-s",
    ]

    tester_process = subprocess.Popen(
        uv_run_pytest_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=tests_dir,
    )

    tester_rc = tester_process.wait()
    assert tester_rc == 0
