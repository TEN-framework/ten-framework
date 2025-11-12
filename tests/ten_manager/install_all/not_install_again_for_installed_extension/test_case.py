#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from .utils import cmd_exec


def check_installed_extensions(app_dir: str) -> bool:
    extension_dir = os.path.join(app_dir, "ten_packages/extension/")
    extensions = os.listdir(extension_dir)

    if len(extensions) != 2:
        return False

    ext_b_path = os.path.join(extension_dir, "ext_b")
    additional_file_path = os.path.join(ext_b_path, "additional_file")

    if os.path.exists(additional_file_path) is False:
        return False

    return True


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

    # The extensions in the dependencies of this app are not in the mock server
    # (i.e., they are not published). Execute 'install_all' in the app, all the
    # dependencies should be installed.
    #
    # We also add 'ext_1' which is in the mock server in the dependencies of
    # this app, to check if tman works well.
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

    assert check_installed_extensions(app_dir) is True


if __name__ == "__main__":
    test_tman_dependency_resolve()
