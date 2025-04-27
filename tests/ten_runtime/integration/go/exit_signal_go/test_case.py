"""
Test exit_signal_go.
"""

import json
import os
import subprocess
import sys
from sys import stdout
import signal
import time
import pytest
from .utils import build_config, build_pkg, fs_utils


@pytest.fixture(scope="session", autouse=True)
def build_and_install_app():
    """Session-scoped fixture to build and install the application once."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "exit_signal_go_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "go"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    # Before starting, cleanup the old app package.
    if build_config_args.ten_enable_integration_tests_prebuilt is False:
        fs_utils.remove_tree(app_root_path)

    if not build_config_args.ten_enable_integration_tests_prebuilt:
        print(f'Assembling and building package "{app_dir_name}".')

        rc = build_pkg.prepare_and_build_app(
            build_config_args,
            root_dir,
            base_path,
            app_dir_name,
            app_language,
        )
        if rc != 0:
            pytest.fail("Failed to build package.")

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
        pytest.fail("Failed to install package.")

    # Skip tests on unsupported platforms
    if sys.platform == "win32":
        pytest.skip("test_exit_signal doesn't support win32")

    # Set environment variables based on platform
    if sys.platform == "darwin":
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path, "exit_signal_go_app/lib"
        )
    else:
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path, "exit_signal_go_app/lib"
        )

        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "exit_signal_go_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )
            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    # Store environment and paths for use in starting the server
    return {
        "env": my_env,
        "app_root_path": app_root_path,
        "server_cmd": os.path.join(base_path, "exit_signal_go_app/bin/start"),
    }


@pytest.fixture
def server(build_and_install_app):
    """Function-scoped fixture to start the server for each test."""
    env = build_and_install_app["env"]
    app_root_path = build_and_install_app["app_root_path"]
    server_cmd = build_and_install_app["server_cmd"]

    if not os.path.isfile(server_cmd):
        pytest.fail(f"Server command '{server_cmd}' does not exist.")

    server_process = subprocess.Popen(
        server_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=env,
        cwd=app_root_path,
    )

    # Wait for the server to start
    time.sleep(3)

    yield server_process

    # Teardown: Ensure the server is terminated
    if server_process.poll() is None:
        server_process.terminate()
        try:
            server_process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_process.kill()


def check_if_extension_stops():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    app_dir = os.path.join(base_dir, "exit_signal_go_app")
    exit_file = os.path.join(app_dir, "exit_signal.json")
    if not os.path.exists(exit_file):
        pytest.fail("Exit file does not exist.")

    with open(exit_file, "r", encoding="utf-8") as f:
        data = json.load(f)
        assert "extension" in data and data["extension"] == "exit_signal"


def test_sigint(server):
    """Test sending SIGINT to the server."""
    server.send_signal(signal.SIGINT)

    server_rc = server.wait()
    print("server:", server_rc)
    assert server_rc == 0

    check_if_extension_stops()


def test_sigterm(server):
    """Test sending SIGTERM to the server."""
    server.send_signal(signal.SIGTERM)

    server_rc = server.wait()
    print("server:", server_rc)
    assert server_rc == 0

    check_if_extension_stops()
