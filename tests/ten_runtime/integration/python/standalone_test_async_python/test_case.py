"""
Test standalone_test_async_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config


def test_standalone_test_async_python():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    extension_root_path = os.path.join(
        base_path, "default_async_extension_python"
    )

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
    # Run standalone test cases.

    # Set the required environment variables for the test.
    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    if sys.platform == "linux":
        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "default_async_extension_python/.ten/app/ten_packages/"
                    "system/ten_runtime/lib/libasan.so"
                ),
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
                base_path,
                (
                    "default_async_extension_python/.ten/app/ten_packages/"
                    "system/ten_runtime/lib/libclang_rt.asan_osx_dynamic.dylib"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["DYLD_INSERT_LIBRARIES"] = libasan_path

    tester_process = subprocess.Popen(
        "tests/bin/start",
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
