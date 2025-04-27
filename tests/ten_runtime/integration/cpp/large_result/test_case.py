"""
Test large_result_app.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import http, build_config, build_pkg, fs_utils


def get_large_result():
    resp = http.post(
        "http://127.0.0.1:8001/",
        {"ten": {"name": "hello_world"}},
    )

    print(len(resp))
    assert len(resp) == 2 * 1024 * 1024 + 2


def test_large_result_app():
    """Test app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "large_result_app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
        server_cmd = os.path.join(
            base_path, "large_result_app/bin/large_result_app.exe"
        )
    elif sys.platform == "darwin":
        server_cmd = os.path.join(
            base_path, "large_result_app/bin/large_result_app"
        )
    else:
        server_cmd = os.path.join(
            base_path, "large_result_app/bin/large_result_app"
        )

    app_dir_name = "large_result_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "cpp"

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

    is_started = http.is_app_started("127.0.0.1", 8001, 30)
    if not is_started:
        print("The large_result_app is not started after 10 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of large_result_app: ", exit_code)

        assert exit_code == 0
        assert False

        return

    try:
        get_large_result()

    finally:
        is_stopped = http.stop_app("127.0.0.1", 8001, 30)

        if not is_stopped:
            print("The large_result_app can not stop after 30 seconds.")
            server.kill()

        exit_code = server.wait()
        print("The exit code of large_result_app: ", exit_code)

        assert exit_code == 0

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
