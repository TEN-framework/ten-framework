#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
"""
Test ffmpeg_basic_app.
"""

import subprocess
import os
import sys
from sys import stdout
from . import video_cmp
from .utils import build_config, build_pkg, fs_utils


def test_ffmpeg_basic_app():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "ffmpeg_basic_app"
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

    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "ffmpeg_basic_app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
        server_cmd = "bin/ffmpeg_basic_app.exe"
    elif sys.platform == "darwin":
        # client depends on some libraries in the TEN app.
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path, "ffmpeg_basic_app/ten_packages/system/ten_runtime/lib"
        )
        server_cmd = "bin/ffmpeg_basic_app"
    else:
        # client depends on some libraries in the TEN app.
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path, "ffmpeg_basic_app/ten_packages/system/ten_runtime/lib"
        )
        server_cmd = "bin/ffmpeg_basic_app"

        if (
            build_config_args.enable_sanitizer
            and not build_config_args.is_clang
        ):
            libasan_path = os.path.join(
                base_path,
                (
                    "ffmpeg_basic_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )
            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

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

    cmp_rc = video_cmp.compareVideo(
        os.path.join(
            app_root_path, "ten_packages/extension/ffmpeg_demuxer/res/test.mp4"
        ),
        os.path.join(
            app_root_path, "ten_packages/extension/ffmpeg_muxer/test.mp4"
        ),
    )
    assert cmp_rc
    # python cv2 would set LD_LIBRARY_PATH to 'cwd', and this will cause the
    # TEN app of the subsequent integration test cases to use the
    # libten_runtime.so' under 'out/<OS>/<CPU>/, rather than the one under
    # '<TEN_app>/lib/'. This is not what TEN runtime expects, so we unset
    # 'LD_LIBRARY_PATH' to prevent from this happening.
    #
    # Refer to ~/.local/lib/python3.10/site-packages/cv2/__init__.py after
    # 'cv2' has been installed.
    #
    # ...
    # os.environ['LD_LIBRARY_PATH'] = ':'.join(l_vars['BINARIES_PATHS']) + ':'
    # + os.environ.get('LD_LIBRARY_PATH', '')
    #                   ^^^^^^^^^^^^^^^   ^^ default to empty, means 'cwd'
    # ...
    try:
        del os.environ["LD_LIBRARY_PATH"]
    except Exception:
        # Maybe 'LD_LIBRARY_PATH' has been unset.
        pass

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        fs_utils.remove_tree(app_root_path)
