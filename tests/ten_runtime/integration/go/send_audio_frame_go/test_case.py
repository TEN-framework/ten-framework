"""
Test send_audio_frame_go.
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


def test_send_audio_frame_go():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "send_audio_frame_go_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "go"

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

    if sys.platform == "win32":
        # client depends on ten_runtime.dll and ten_utils.dll in the TEN app.
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "send_audio_frame_go_app/ten_packages/system/ten_runtime/lib",
            )
            + os.pathsep
            + my_env.get("PATH", "")
        )
    elif sys.platform == "darwin":
        # client depends on some libraries in the TEN app.
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "send_audio_frame_go_app/ten_packages/system/ten_runtime/lib",
        )
    else:
        # client depends on some libraries in the TEN app.
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "send_audio_frame_go_app/ten_packages/system/ten_runtime/lib",
        )

        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "send_audio_frame_go_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )
            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    if sys.platform == "win32":
        start_py = os.path.join(base_path, "send_audio_frame_go_app/bin/start.py")
        server_cmd = [sys.executable, start_py]

        if not os.path.isfile(start_py):
            print(f"Server command '{start_py}' does not exist.")
            assert False
    else:
        server_cmd = os.path.join(base_path, "send_audio_frame_go_app/bin/start")

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
        print("The send_audio_frame_go is not started after 30 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of send_audio_frame_go: ", exit_code)

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
            print("The send_audio_frame_go can not stop after 30 seconds.")
            server.kill()

        exit_code = server.wait()
        print("The exit code of send_audio_frame_go: ", exit_code)

        assert exit_code == 0

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
