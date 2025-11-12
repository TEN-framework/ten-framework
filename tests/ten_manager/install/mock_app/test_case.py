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


def analyze_resolve_result(app_root_folder: str) -> None:
    extension_folder = os.path.join(
        app_root_folder, "ten_packages", "extension"
    )

    with open(
        os.path.join(app_root_folder, "expected.json"),
        "r",
        encoding="utf-8",
    ) as expected_json_file:
        expected_json = json.load(expected_json_file)

        # @{
        # Check all expected extensions are installed.
        for expected_dep_in_app_json in expected_json["extension"]:
            found_in_folder = False
            found_in_manifest_deps = False

            for dir_item in os.listdir(extension_folder):
                if dir_item == expected_dep_in_app_json["name"]:
                    found_in_folder = True
                    with open(
                        os.path.join(
                            extension_folder,
                            expected_dep_in_app_json["name"],
                            "manifest.json",
                        ),
                        "r",
                        encoding="utf-8",
                    ) as ext_manifest_file:
                        ext_manifest_json = json.load(ext_manifest_file)
                        assert (
                            ext_manifest_json["name"]
                            == expected_dep_in_app_json["name"]
                        )
                        assert expected_dep_in_app_json["version"].endswith(
                            ext_manifest_json["version"]
                        )
                    break

            with open(
                os.path.join(app_root_folder, "manifest.json"),
                "r",
                encoding="utf-8",
            ) as app_manifest_file:
                app_manifest_json = json.load(app_manifest_file)
                for dep_in_app_json in app_manifest_json["dependencies"]:
                    if (
                        dep_in_app_json["name"]
                        == expected_dep_in_app_json["name"]
                    ):
                        found_in_manifest_deps = True

                        assert (
                            dep_in_app_json["version"]
                            == expected_dep_in_app_json["version"]
                        )
                        break

            assert found_in_folder is True
            assert found_in_manifest_deps is True
        # @}

        # @{
        # Check there is no other unexpected extensions be installed.
        installed_extension_cnt = 0
        if os.path.exists(extension_folder):
            for dir_item in os.listdir(extension_folder):
                installed_extension_cnt += 1
        assert len(expected_json["extension"]) == installed_extension_cnt
        # @}


def test_tman_install():
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

    mock_app_path = os.path.join(base_path, "mock_app")
    os.chdir(mock_app_path)

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
            "extension",
            "ext_a",
            "--os=linux",
            "--arch=x64",
        ],
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    # Install ext_b with specific version.
    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
            "extension",
            "ext_b@0.2.6",
            "--os=linux",
            "--arch=x64",
        ],
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    analyze_resolve_result(mock_app_path)
