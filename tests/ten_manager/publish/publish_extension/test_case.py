#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import json
from .utils import cmd_exec


def get_package_name():
    base_path = os.path.dirname(os.path.abspath(__file__))
    manifest = os.path.join(base_path, "mock_extension/manifest.json")
    if os.path.exists(manifest):
        with open(manifest, "r", encoding="utf-8") as f_mani:
            manifest_json = json.load(f_mani)

            items = [
                manifest_json["name"],
                manifest_json["version"],
            ]
            return "_".join(items)
    else:
        raise FileNotFoundError("manifest.json not exists.")


def test_tman_publish():
    """Test tman publish."""
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

    mock_extension_path = os.path.join(base_path, "mock_extension")
    os.chdir(mock_extension_path)

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            "--yes",
            f"--config-file={config_file}",
            "publish",
        ],
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    assert returncode == 0

    package_name = get_package_name()

    dest_folder_path = os.path.join(
        root_dir,
        "tests/local_registry/extension/mock_extension/0.0.1",
    )
    files_and_directories = os.listdir(dest_folder_path)
    files = [
        f
        for f in files_and_directories
        if os.path.isfile(os.path.join(dest_folder_path, f))
    ]
    if files:
        first_file_name = files[0]
        if not first_file_name.startswith(package_name):
            assert False
    else:
        assert False
