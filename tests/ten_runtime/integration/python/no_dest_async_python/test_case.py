"""
Test no_dest_async_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import http, build_config, build_pkg, fs_utils


def http_request():
    return http.post(
        "http://127.0.0.1:8002/",
        {
            "ten": {
                "name": "test",
            },
        },
    )


def test_no_dest_async_python():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    # Set the required environment variables for the test.
    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    app_dir_name = "no_dest_async_python_app"
    app_root_path = os.path.join(base_path, app_dir_name)
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
    rc = build_pkg.bootstrap_python_dependencies(app_root_path, my_env, log_level=1)
    if rc != 0:
        assert False, "Failed to bootstrap Python dependencies."

    # Step 2: Setup AddressSanitizer if needed
    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "no_dest_async_python_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    # Step 3: Start the server
    main_py_path = os.path.join(app_root_path, "main.py")
    if not os.path.isfile(main_py_path):
        print(f"main.py not found at '{main_py_path}'.")
        assert False

    # When sanitizer is enabled, we need to bypass `uv run` because `uv` itself
    # may trigger memory leak reports (false positives from the tool itself),
    # causing the test to fail.
    if sys.platform == "linux" and build_config_args.enable_sanitizer:
        print("Starting server with python from venv (bypassing uv run)...")
        venv_path = os.path.join(app_root_path, ".venv")
        python_exe = os.path.join(venv_path, "bin", "python")
        server_cmd = [python_exe, "main.py"]
        my_env["VIRTUAL_ENV"] = venv_path
    else:
        print("Starting server with uv run main.py...")
        server_cmd = ["uv", "run", "main.py"]

    server = subprocess.Popen(
        server_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )

    is_started = http.is_app_started("127.0.0.1", 8002, 30)
    if not is_started:
        print("The no_dest_async_python is not started after 30 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of no_dest_async_python: ", exit_code)

        assert exit_code == 0
        assert False

        return

    try:
        resp = http_request()
        assert resp != 500
        print(resp)

    finally:
        is_stopped = http.stop_app("127.0.0.1", 8002, 30)
        if not is_stopped:
            print("The no_dest_async_python can not stop after 30 seconds.")
            server.kill()

        exit_code = server.wait()
        print("The exit code of no_dest_async_python: ", exit_code)

        assert exit_code == 0

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
