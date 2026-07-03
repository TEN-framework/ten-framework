#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import json
from .utils import cmd_exec, fs_utils


def test_tman_install_app():
    """Test tman install."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../")

    my_env = os.environ.copy()
    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(root_dir, "ten_manager/lib") + ";" + my_env["PATH"]
        )
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman.exe")
    else:
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman")

    os.chdir(base_path)

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )
    app_dir = os.path.join(base_path, "app_1")

    # Make sure the app directory is not exist.
    if os.path.exists(app_dir):
        fs_utils.remove_tree(app_dir)

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
            "app",
            "app_1",
        ],
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    # Check if the app is installed.

    assert os.path.exists(app_dir)

    os.chdir(app_dir)

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
        ],
    )

    if returncode != 0:
        print(output_text)
        assert False
