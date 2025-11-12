"""
Test long_running_app.
"""

import subprocess
import os
import sys
from sys import stdout
from utils import build_config, build_pkg, fs_utils


def test_long_running_app():
    """Test app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "long_running_app"
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

    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "long_running_app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
        server_cmd = os.path.join(
            base_path, "long_running_app/bin/long_running_app.exe"
        )
    elif sys.platform == "darwin":
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "long_running_app/ten_packages/system/ten_runtime/lib",
        )
        server_cmd = os.path.join(
            base_path, "long_running_app/bin/long_running_app"
        )
    else:
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "long_running_app/ten_packages/system/ten_runtime/lib",
        )
        server_cmd = os.path.join(
            base_path, "long_running_app/bin/long_running_app"
        )

        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "long_running_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )
            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

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


if __name__ == "__main__":
    test_long_running_app()
