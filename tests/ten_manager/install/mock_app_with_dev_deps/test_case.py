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


def analyze_resolve_result(
    app_root_folder: str, expected_json_file_name: str
) -> None:
    with open(
        os.path.join(app_root_folder, expected_json_file_name),
        "r",
        encoding="utf-8",
    ) as expected_json_file:
        expected_json = json.load(expected_json_file)

        check_package_types = ["extension", "system"]
        ten_packages_root = os.path.join(app_root_folder, "ten_packages")

        # Get actual folders under ten_packages
        actual_package_folders = set()
        if os.path.exists(ten_packages_root):
            for item in os.listdir(ten_packages_root):
                item_path = os.path.join(ten_packages_root, item)
                if os.path.isdir(item_path) and item in check_package_types:
                    actual_package_folders.add(item)

        for check_package_type in check_package_types:
            # Check if this type exists in expected_json and is not empty
            has_expected_packages = (
                check_package_type in expected_json
                and expected_json[check_package_type]
                and len(expected_json[check_package_type]) > 0
            )

            # Check if corresponding folder exists under ten_packages
            has_actual_folder = check_package_type in actual_package_folders

            package_folder = os.path.join(ten_packages_root, check_package_type)

            if has_expected_packages and not has_actual_folder:
                assert False, (
                    f"Expected {check_package_type} packages but folder "
                    f"{package_folder} does not exist"
                )

            if not has_expected_packages and has_actual_folder:
                assert False, (
                    f"Found unexpected {check_package_type} folder "
                    f"{package_folder} but no packages expected in JSON"
                )

            # Skip if neither expected packages nor actual folder exists
            if not has_expected_packages and not has_actual_folder:
                continue

            # At this point, both has_expected_packages and has_actual_folder
            # are True
            expected_packages = expected_json[check_package_type]

            # @{
            # Check all expected packages are installed.
            for expected_dep_in_app_json in expected_packages:
                found_in_folder = False
                found_in_manifest_deps = False

                for dir_item in os.listdir(package_folder):
                    if dir_item == expected_dep_in_app_json["name"]:
                        found_in_folder = True
                        with open(
                            os.path.join(
                                package_folder,
                                expected_dep_in_app_json["name"],
                                "manifest.json",
                            ),
                            "r",
                            encoding="utf-8",
                        ) as package_manifest_file:
                            package_manifest_json = json.load(
                                package_manifest_file
                            )
                            assert (
                                package_manifest_json["name"]
                                == expected_dep_in_app_json["name"]
                            )
                            assert expected_dep_in_app_json["version"].endswith(
                                package_manifest_json["version"]
                            )
                        break

                # Check dependencies in application manifest file
                with open(
                    os.path.join(app_root_folder, "manifest.json"),
                    "r",
                    encoding="utf-8",
                ) as app_manifest_file:
                    app_manifest_json = json.load(app_manifest_file)

                    # Check both dependencies and dev_dependencies fields
                    all_deps = []
                    if "dependencies" in app_manifest_json:
                        all_deps.extend(app_manifest_json["dependencies"])
                    if "dev_dependencies" in app_manifest_json:
                        all_deps.extend(app_manifest_json["dev_dependencies"])

                    for dep_in_app_json in all_deps:
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

                assert found_in_folder is True, (
                    f"Package {expected_dep_in_app_json['name']} not found in "
                    f"folder {package_folder}"
                )
                assert found_in_manifest_deps is True, (
                    f"Package {expected_dep_in_app_json['name']} not found in "
                    f"manifest dependencies"
                )
            # @}

            # @{
            # Check there is no other unexpected packages be installed.
            installed_package_cnt = 0
            for dir_item in os.listdir(package_folder):
                installed_package_cnt += 1

            assert len(expected_packages) == installed_package_cnt, (
                f"Expected {len(expected_packages)} {check_package_type} "
                f"packages, but found {installed_package_cnt}"
            )
            # @}


def __test_tman_install(prod: bool):
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

    mock_app_with_dev_deps_path = os.path.join(
        base_path, "mock_app_with_dev_deps"
    )
    os.chdir(mock_app_with_dev_deps_path)

    # If the ten_packages folder exists, remove it.
    ten_packages_folder = os.path.join(
        mock_app_with_dev_deps_path, "ten_packages"
    )
    fs_utils.remove_tree(ten_packages_folder)

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    install_cmd = [
        tman_bin,
        f"--config-file={config_file}",
        "--yes",
        "install",
        "extension",
        "ext_a",
        "--os=linux",
        "--arch=x64",
    ]

    if prod:
        install_cmd.append("--production")

    # No production mode by default.
    returncode, output_text = cmd_exec.run_cmd_realtime(
        install_cmd,
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    install_cmd = [
        tman_bin,
        f"--config-file={config_file}",
        "--yes",
        "install",
        "extension",
        "ext_b@0.2.6",
        "--os=linux",
        "--arch=x64",
    ]

    if prod:
        install_cmd.append("--production")

    # Install ext_b with specific version.
    returncode, output_text = cmd_exec.run_cmd_realtime(
        install_cmd,
        env=my_env,
    )
    if returncode != 0:
        print(output_text)
        assert False

    expected_json_file_name = "expected_no_prod.json"
    if prod:
        expected_json_file_name = "expected_prod.json"

    analyze_resolve_result(mock_app_with_dev_deps_path, expected_json_file_name)


def test_tman_install_with_dev_deps():
    __test_tman_install(False)
    __test_tman_install(True)
