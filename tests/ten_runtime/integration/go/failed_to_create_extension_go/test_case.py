"""
Test failed_to_create_extension_go.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import msgpack, build_config, build_pkg, fs_utils


def test_failed_to_create_extension_go():
    """Test app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "failed_to_create_extension_go_app"
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
        # server (main.exe) depends on ten_runtime.dll and ten_utils.dll in the TEN app.
        my_env["PATH"] = (
            os.path.join(
                base_path,
                (
                    "failed_to_create_extension_go_app/ten_packages/"
                    "system/ten_runtime/lib"
                ),
            )
            + os.pathsep
            + my_env.get("PATH", "")
        )
        start_py = os.path.join(base_path, "failed_to_create_extension_go_app/bin/start.py")
        server_cmd = [sys.executable, start_py]

        if not os.path.isfile(start_py):
            print(f"Server command '{start_py}' does not exist.")
            assert False
    else:
        server_cmd = os.path.join(
            base_path, "failed_to_create_extension_go_app/bin/start"
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

    server_rc = server.wait()
    print("server: ", server_rc)
    assert server_rc == 0

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        fs_utils.remove_tree(app_root_path)
