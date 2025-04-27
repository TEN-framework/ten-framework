#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
import sys
from .utils import cmd_exec


def analyze_resolve_result(app_root_folder: str) -> None:
    extension_folder = os.path.join(
        app_root_folder, "ten_packages", "extension"
    )

    with open(
        os.path.join(app_root_folder, "expected.json"), "r", encoding="utf-8"
    ) as expected_json_file:
        expected_json = json.load(expected_json_file)

        # @{
        # Check all expected extensions are installed.
        for ext in expected_json["extension"]:
            found_in_folder = False

            for dir_item in os.listdir(extension_folder):
                if dir_item == ext["name"]:
                    found_in_folder = True
                    with open(
                        os.path.join(
                            extension_folder, ext["name"], "manifest.json"
                        ),
                        "r",
                        encoding="utf-8",
                    ) as ext_manifest_file:
                        ext_manifest_json = json.load(ext_manifest_file)
                        assert ext_manifest_json["name"] == ext["name"]
                        assert ext_manifest_json["version"] == ext["version"]
                    break

            assert found_in_folder is True
        # @}

        # @{
        # Check there is no other unexpected extensions be installed.
        installed_extension_cnt = 0
        if os.path.exists(extension_folder):
            for dir_item in os.listdir(extension_folder):
                installed_extension_cnt += 1
        assert len(expected_json["extension"]) == installed_extension_cnt
        # @}


def test_tman_dependency_resolve():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../")

    if sys.platform == "win32":
        os.environ["PATH"] = (
            os.path.join(root_dir, "ten_manager/lib") + ";" + os.getenv("PATH")
        )
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman.exe")
    else:
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman")

    app_dir = os.path.join(base_path, "test_app")

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    # execute tman install.
    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
        ],
        cwd=app_dir,
    )
    if returncode != 0:
        print(output_text)
        assert False

    analyze_resolve_result(app_dir)


if __name__ == "__main__":
    test_tman_dependency_resolve()
