"""
Test standalone_test_2_go.
"""

import platform
import subprocess
import os
import sys
from sys import stdout
from .utils import build_config


def is_mac_arm64() -> bool:
    return (
        platform.system().lower() == "darwin"
        and platform.machine().lower() == "arm64"
    )


def test_standalone_test_2_go():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    extension_root_path = os.path.join(base_path, "default_extension_go")

    # Step 1:
    #
    # Install all the dependencies of the default_extension_go package.
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
    # Run the test.
    if sys.platform == "win32":
        test_cmd = [
            sys.executable,
            "tests/bin/start.py"
        ]
    else:
        test_cmd = [
            "tests/bin/start",
        ]

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    if build_config_args.enable_sanitizer and not is_mac_arm64():
        test_cmd.append("-asan")

    if sys.platform == "win32":
        # On Windows, Go always uses MinGW toolchain (CGO requires GCC).
        # We need to find and use MinGW gcc/g++ explicitly.
        mingw_gcc = build_config.find_mingw_gcc()
        mingw_gxx = build_config.find_mingw_gxx()
        if mingw_gcc and mingw_gxx:
            my_env["CC"] = mingw_gcc
            my_env["CXX"] = mingw_gxx
        else:
            my_env["CC"] = "gcc"
            my_env["CXX"] = "g++"
    elif build_config_args.is_clang:
        my_env["CC"] = "clang"
        my_env["CXX"] = "clang++"
    else:
        my_env["CC"] = "gcc"
        my_env["CXX"] = "g++"

    tester_process = subprocess.Popen(
        test_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
