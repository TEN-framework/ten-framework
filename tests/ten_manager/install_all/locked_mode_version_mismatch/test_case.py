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


def test_tman_install_locked_version_mismatch():
    """Test tman install --locked fails when locked version doesn't satisfy semver."""
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

    support_file = os.path.join(app_dir, "supports.json")

    support_data = {}
    if os.path.exists(support_file):
        with open(support_file, "r", encoding="utf-8") as file:
            support_data = json.load(file)

    command = [
        tman_bin,
        f"--config-file={config_file}",
        "--yes",
        "install",
        "--locked",
    ]

    if "os" in support_data:
        command.append(f"--os={support_data['os']}")
    if "arch" in support_data:
        command.append(f"--arch={support_data['arch']}")

    # Execute tman install --locked (should fail).
    returncode, output_text = cmd_exec.run_cmd_realtime(
        command,
        cwd=app_dir,
    )

    # Should fail
    assert returncode != 0, \
        "tman install --locked should fail when locked version doesn't satisfy semver"

    # Check error message
    assert "does not satisfy" in output_text or "version requirement" in output_text, \
        f"Error message should mention version mismatch. Got: {output_text}"
    assert "lock file is out of sync" in output_text or "without --locked" in output_text, \
        f"Error message should suggest updating lock file. Got: {output_text}"


if __name__ == "__main__":
    test_tman_install_locked_version_mismatch()
