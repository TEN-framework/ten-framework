#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import json
from .utils import cmd_exec, fs_utils, build_config


def get_package_name() -> str:
    base_path = os.path.dirname(os.path.abspath(__file__))
    manifest = os.path.join(base_path, "default_extension_cpp/manifest.json")
    if os.path.exists(manifest):
        with open(manifest, "r", encoding="utf-8") as f_mani:
            manifest_json = json.load(f_mani)
            return manifest_json["name"]
    else:
        raise FileNotFoundError("manifest.json not exists.")


def test_tman_fetch_extension() -> None:
    """Test tman fetch."""
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

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            "--yes",
            f"--config-file={config_file}",
            "fetch",
            "extension",
            "default_extension_cpp",
        ],
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    assert returncode == 0

    package_name = get_package_name()
    assert package_name == "default_extension_cpp"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        extension_dir = os.path.join(base_path, "default_extension_cpp")
        fs_utils.remove_tree(extension_dir)
