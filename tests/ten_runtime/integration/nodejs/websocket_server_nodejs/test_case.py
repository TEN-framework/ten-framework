"""
Test websocket_server_nodejs.
"""

import subprocess
import os
import sys
import time
import websocket
from sys import stdout
from .utils import http, build_config, build_pkg, fs_utils


# In this test case, there is no predefined startup order for the two
# extensions:
#
# `http_server_extension_nodejs` (HTTP server)
# `websocket_server_nodejs` (WebSocket server)
#
# As a result, the HTTP server extension might start successfully first,
# followed by the WebSocket server extension. In this scenario, the client may
# detect that the HTTP server is available and attempt to connect to the
# WebSocket immediately, leading to a connection failure. Therefore, a retry
# mechanism is needed to make the process more robust.
def ws_request():
    """Send websocket request with retry mechanism."""

    max_retries = 3
    attempt = 0

    while attempt < max_retries:
        try:
            ws = websocket.create_connection("ws://localhost:8007")
            ws.send("Hello, World!")
            result = ws.recv()
            ws.close()
            return result
        except (websocket.WebSocketException, ConnectionError) as e:
            print(f"Attempt {attempt + 1} failed: {e}")
            attempt += 1
            time.sleep(1)

    raise ConnectionError("Failed to connect after 3 attempts")


def test_websocket_server_nodejs():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "websocket_server_nodejs_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "nodejs"

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

        # Compile typescript extensions.
        rc = build_pkg.build_nodejs_extensions(app_root_path)
        if rc != 0:
            assert False, "Failed to build TypeScript extensions."

    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "websocket_server_nodejs_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    server_cmd = os.path.join(
        base_path, "websocket_server_nodejs_app/bin/start"
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
        print("The websocket_server_nodejs is not started after 10 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of websocket_server_nodejs: ", exit_code)

        assert exit_code == 0
        assert False

    try:
        print("Sending ws message to the app server.")
        resp = ws_request()
        assert resp == "Echo: Hello, World!"
    finally:
        is_stopped = http.stop_app("127.0.0.1", 8002, 30)
        if not is_stopped:
            print("The websocket_server_nodejs can not stop after 30 seconds.")
            server.kill()

        exit_code = server.wait()
        print("The exit code of websocket_server_nodejs: ", exit_code)

        assert exit_code == 0

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
