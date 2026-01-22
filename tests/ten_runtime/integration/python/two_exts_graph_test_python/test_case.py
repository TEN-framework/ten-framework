"""
Test two_exts_graph_test_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, build_pkg, fs_utils


def test_two_exts_graph_test_python():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    # Set the required environment variables for the test.
    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    app_dir_name = "two_exts_graph_test_python_app"
    app_root_path = os.path.join(base_path, "two_exts_graph_test_python_app")
    app_language = "python"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    # Before starting, cleanup the old app package.
    fs_utils.remove_tree(app_root_path)

    print(f'Assembling and building package "{app_dir_name}".')

    rc = build_pkg.prepare_and_build_app(
        build_config_args,
        root_dir,
        base_path,
        app_dir_name,
        app_language,
    )
    if rc != 0:
        assert False, "Failed to build package."

    # Step 1: Bootstrap Python dependencies (update pyproject.toml and sync)
    print("Bootstrapping Python dependencies...")
    rc = build_pkg.bootstrap_python_dependencies(
        app_root_path, my_env, log_level=1
    )
    if rc != 0:
        assert False, "Failed to bootstrap Python dependencies."

    # Step 2: Setup AddressSanitizer if needed
    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "two_exts_graph_test_python_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    tests_dir = os.path.join(app_root_path, "tests")

    # Step 3: Sync dependencies in tests directory
    #
    # Run uv sync --all-packages to install dependencies in tests directory.
    # This is needed to create .venv/bin/python for sanitizer mode.
    print("Syncing dependencies in tests directory...")
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
    # Run the test using uv run pytest.
    # When sanitizer is enabled, we need to bypass `uv run` because `uv` itself
    # may trigger memory leak reports (false positives from the tool itself),
    # causing the test to fail.
    if sys.platform == "linux" and build_config_args.enable_sanitizer:
        print("Starting pytest with python from venv (bypassing uv run)...")
        venv_path = os.path.join(tests_dir, ".venv")
        python_exe = os.path.join(venv_path, "bin", "python")
        uv_run_pytest_cmd = [python_exe, "-m", "pytest", "-s"]
        my_env["VIRTUAL_ENV"] = venv_path
    else:
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
        if build_config_args.ten_enable_tests_cleanup is True:
            fs_utils.remove_tree(app_root_path)
