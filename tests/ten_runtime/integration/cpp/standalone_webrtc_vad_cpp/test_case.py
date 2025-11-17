"""
Test standalone_webrtc_vad_cpp.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, fs_utils


def test_standalone_webrtc_vad_cpp():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "webrtc_vad_cpp")
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Determine tman path based on platform
    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(root_dir, "ten_manager/lib") + ";" + my_env["PATH"]
        )
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman.exe")
    else:
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman")

    # Step 1:
    #
    # Create webrtc_vad_cpp package directly.
    tman_fetch_cmd = [
        tman_bin,
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "fetch",
        "extension",
        "webrtc_vad_cpp",
    ]

    tman_fetch_process = subprocess.Popen(
        tman_fetch_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=base_path,
    )
    tman_fetch_process.wait()
    return_code = tman_fetch_process.returncode
    if return_code != 0:
        assert False, "Failed to fetch package."

    # Step 2:
    #
    # Install all the dependencies of the webrtc_vad_cpp package.
    tman_install_cmd = [
        tman_bin,
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

    # Step 3:
    #
    # Build the extension.
    print("Building the extension.")

    tman_run_build_cmd = [
        tman_bin,
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "run",
        "build",
    ]

    tman_run_build_process = subprocess.Popen(
        tman_run_build_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tman_run_build_process.wait()
    return_code = tman_run_build_process.returncode
    if return_code != 0:
        assert False, "Failed to build package."

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    # Step 4:
    #
    # Standalone testing the extension.
    print("Testing the extension.")

    # Determine test executable path based on platform
    if sys.platform == "win32":
        # On Windows, add the required DLL paths to PATH
        my_env["PATH"] = (
            os.path.join(
                extension_root_path,
                ".ten/app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
        standalone_test_cmd = [
            os.path.join(extension_root_path, "bin/webrtc_vad_cpp_test.exe"),
        ]
    else:
        standalone_test_cmd = [
            os.path.join(extension_root_path, "bin/webrtc_vad_cpp_test"),
        ]

    standalone_test_process = subprocess.Popen(
        standalone_test_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    standalone_test_process.wait()
    return_code = standalone_test_process.returncode
    if return_code != 0:
        assert False, "Failed to test package."

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        fs_utils.remove_tree(extension_root_path)
