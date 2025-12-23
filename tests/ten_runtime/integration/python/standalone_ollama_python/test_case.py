"""
Test standalone_ollama_async_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, build_pkg, fs_utils


def test_standalone_ollama_async_python():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "ollama_python")
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create ollama_python package directly.
    tman_create_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "create",
        "extension",
        "ollama_python",
        "--template",
        "ollama_python",
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
    # Install all the dependencies.
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

    # Step 3:
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

    # Step 4:
    #
    # Set the required environment variables for the test.

    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

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

    # Step 5:
    #
    # Run the test using uv run pytest.
    uv_run_pytest_cmd = [
        "uv",
        "run",
        "pytest",
        "-s",
    ]

    try:
        tester_process = subprocess.Popen(
            uv_run_pytest_cmd,
            stdout=stdout,
            stderr=subprocess.STDOUT,
            env=my_env,
            cwd=tests_dir,
        )

        tester_rc = tester_process.wait()
        assert tester_rc == 0
    finally:
        venv_path = os.path.join(tests_dir, ".venv")
        if os.path.exists(venv_path):
            fs_utils.remove_tree(venv_path)
