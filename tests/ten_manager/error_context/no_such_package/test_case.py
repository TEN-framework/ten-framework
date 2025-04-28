#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from .utils import cmd_exec


def test_invalid_package_type():
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
    origin_wd = os.getcwd()

    try:
        os.chdir(app_dir)

        config_file = os.path.join(
            root_dir,
            "tests/local_registry/config.json",
        )

        cmds = [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
            "extension",
            "package_does_not_exist",
        ]
        returncode, _ = cmd_exec.get_cmd_output(" ".join(cmds))
        assert returncode == 1
    finally:
        os.chdir(origin_wd)


if __name__ == "__main__":
    test_invalid_package_type()
