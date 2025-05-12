"""
Test standalone_test_go.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, fs_utils


def test_standalone_test_go():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "default_extension_go")
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create default_extension_go package directly.
    tman_create_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "create",
        "extension",
        "default_extension_go",
        "--template",
        "default_extension_go",
    ]

    tman_create_process = subprocess.Popen(
        tman_create_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=base_path,
    )
    tman_create_process.wait()
    return_code = tman_create_process.returncode
    if return_code != 0:
        assert False, "Failed to create package."

    # Step 2:
    #
    # Install all the dependencies of the ollama_python package.
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


    if sys.platform == "win32":
        print("test_standalone_test_go doesn't support win32")
        assert False
    elif sys.platform == "darwin":
        # client depends on some libraries in the TEN app.
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "standalone_test_go_app/ten_packages/system/ten_runtime/lib",
        )
    else:
        # client depends on some libraries in the TEN app.
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "standalone_test_go_app/ten_packages/system/ten_runtime/lib",
        )

        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "standalone_test_go_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )
            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    # Step 3:
    #
    # Run the test.
    tester_process = subprocess.Popen(
        "tests/bin/start",
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
