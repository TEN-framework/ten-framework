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


def analyze_resolve_result(test_app_root_folder: str) -> None:
    extension_folder = os.path.join(
        test_app_root_folder, "ten_packages", "extension"
    )

    with open(
        os.path.join(test_app_root_folder, "expected.json"),
        "r",
        encoding="utf-8",
    ) as expected_json_file:
        print(test_app_root_folder)
        expected_json = json.load(expected_json_file)

        # @{
        # Check all expected extensions are installed.
        for ext in expected_json["extension"]:
            found = False
            for dir_item in os.listdir(extension_folder):
                if dir_item == ext["name"]:
                    found = True
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

                        if hasattr(ext, "supports"):
                            assert (
                                ext_manifest_json["supports"] == ext["supports"]
                            )

                    break
            assert found is True
        # @}

        # @{
        # Check there is no other unexpected extensions be installed.
        installed_extension_cnt = 0
        if os.path.exists(extension_folder):
            for dir_item in os.listdir(extension_folder):
                installed_extension_cnt += 1
        assert len(expected_json["extension"]) == installed_extension_cnt
        # @}


def run_test(tman_bin: str, root_dir: str, test_app_root_folder: str) -> None:
    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    support_file = os.path.join(test_app_root_folder, "supports.json")

    support_data = {}
    if os.path.exists(support_file):
        with open(support_file, "r", encoding="utf-8") as file:
            support_data = json.load(file)

    command = [
        tman_bin,
        f"--config-file={config_file}",
        "--yes",
        "install",
    ]

    if "os" in support_data:
        command.append(f"--os={support_data['os']}")
    if "arch" in support_data:
        command.append(f"--arch={support_data['arch']}")

    returncode, output_text = cmd_exec.run_cmd_realtime(
        command,
        cwd=test_app_root_folder,
    )

    if returncode:
        print("Failed to execute `tman install` command.\n")
        print(output_text)
    else:
        print(output_text)

    analyze_resolve_result(test_app_root_folder)


def test_tman_dependency_resolve():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../")

    if sys.platform == "win32":
        os.environ["PATH"] = (
            os.path.join(root_dir, "ten_manager/lib") + ";" + os.getenv("PATH")
        )
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman.exe")
    else:
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman")

    test_case_root_path = os.path.join(base_path, "test_cases")
    test_cases = os.listdir(test_case_root_path)
    for test_case in test_cases:
        print(test_case)
        app_root_folder = os.path.join(test_case_root_path, test_case)
        run_test(tman_bin, root_dir, app_root_folder)


if __name__ == "__main__":
    test_tman_dependency_resolve()
