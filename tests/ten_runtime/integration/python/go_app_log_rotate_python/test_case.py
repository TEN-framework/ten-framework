"""
Test go_app_log_rotate_python.
"""

import signal
import subprocess
import os
import sys
from sys import stdout
import time
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


def test_go_app_log_rotate_python():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    # Create virtual environment.
    venv_dir = os.path.join(base_path, "venv")
    subprocess.run([sys.executable, "-m", "venv", venv_dir], check=True)

    my_env = os.environ.copy()

    # Set the required environment variables for the test.
    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    # Launch virtual environment.
    my_env["VIRTUAL_ENV"] = venv_dir
    my_env["PATH"] = os.path.join(venv_dir, "bin") + os.pathsep + my_env["PATH"]

    if sys.platform == "win32":
        print("test_go_app_log_rotate_python doesn't support win32")
        assert False
    elif sys.platform == "darwin":
        # client depends on some libraries in the TEN app.
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path, "go_app_log_rotate_python_app/lib"
        )
    else:
        # client depends on some libraries in the TEN app.
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path, "go_app_log_rotate_python_app/lib"
        )

    app_dir_name = "go_app_log_rotate_python_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "go"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    if build_config_args.ten_enable_integration_tests_prebuilt is False:
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

    tman_install_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "install",
    ]

    tman_install_process = subprocess.Popen(
        tman_install_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )
    tman_install_process.wait()
    return_code = tman_install_process.returncode
    if return_code != 0:
        assert False, "Failed to install package."

    bootstrap_cmd = os.path.join(
        base_path, "go_app_log_rotate_python_app/bin/bootstrap"
    )

    bootstrap_process = subprocess.Popen(
        bootstrap_cmd, stdout=stdout, stderr=subprocess.STDOUT, env=my_env
    )
    bootstrap_process.wait()

    if sys.platform == "linux":
        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "go_app_log_rotate_python_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    server_cmd = os.path.join(
        base_path, "go_app_log_rotate_python_app/bin/start"
    )

    if not os.path.isfile(server_cmd):
        print(f"Server command '{server_cmd}' does not exist.")
        assert False

    server = subprocess.Popen(
        server_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )

    is_started = http.is_app_started("127.0.0.1", 8002, 30)
    if not is_started:
        print("The go_app_log_rotate_python is not started after 10 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of go_app_log_rotate_python: ", exit_code)

        assert exit_code == 0
        assert False

        return

    # print the pid of the server
    print(f"Server PID: {server.pid}")

    # sleep 3 seconds
    time.sleep(3)

    # rename the log file
    os.rename(
        os.path.join(app_root_path, "test.log"),
        os.path.join(app_root_path, "test.log.1"),
    )

    # send sighup to the server
    os.kill(server.pid, signal.SIGHUP)

    # sleep 3 seconds
    time.sleep(3)

    # rename the log file
    os.rename(
        os.path.join(app_root_path, "test.log"),
        os.path.join(app_root_path, "test.log.2"),
    )

    # send sighup to the server
    os.kill(server.pid, signal.SIGHUP)

    try:
        resp = http_request()
        assert resp != 500
        print(resp)

    finally:
        is_stopped = http.stop_app("127.0.0.1", 8002, 30)
        if not is_stopped:
            print("The go_app_log_rotate_python can not stop after 30 seconds.")
            server.kill()

        # Print log file contents regardless of test outcome.
        try:
            # Wait with a 10-minute timeout
            exit_code = server.wait(timeout=600)
            print("The exit code of go_app_log_rotate_python: ", exit_code)
        except subprocess.TimeoutExpired:
            print("TIMEOUT: server.wait() blocked for more than 10 minutes")
            # Log dump will happen in the finally block.
            # Force terminate the server after timeout.
            server.kill()
            exit_code = None

        # check the log file
        assert os.path.exists(os.path.join(app_root_path, "test.log"))
        assert os.path.exists(os.path.join(app_root_path, "test.log.1"))
        assert os.path.exists(os.path.join(app_root_path, "test.log.2"))

        # Print log file contents regardless of test outcome.
        for log_file in ["test.log.1", "test.log.2", "test.log"]:
            file_path = os.path.join(app_root_path, log_file)
            print(f"\n----- Contents of {log_file} -----")
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    print(f.read())
            except Exception as e:
                print(f"Error reading {log_file}: {e}")
            print(f"----- End of {log_file} -----\n")

        # Fail the test if we hit a timeout.
        if exit_code is None:
            assert (
                False
            ), "Test failed due to timeout waiting for server to exit"
        else:
            assert exit_code == 0

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
            fs_utils.remove_tree(venv_dir)
