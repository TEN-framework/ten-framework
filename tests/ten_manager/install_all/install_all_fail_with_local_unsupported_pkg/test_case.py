#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from .utils import cmd_exec


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

    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--yes",
            "install",
            "--os=linux",
        ],
        cwd=app_dir,
    )

    print(returncode)
    print(output_text)

    assert returncode != 0
    # As the local package 'heiheihei_1' is not compatible with linux os, and
    # the package cannot be found in the registry, so no candidates can be found
    # and the installation fails.
    #
    # TODO(Wei): The `output_text` might be `🔍  Resolving packages...\n`. It
    # means the error output might not capture the desired error line. We need
    # to check how to obtain it completely.
    # assert output_text.find("Failed to find candidates for extension") != -1


if __name__ == "__main__":
    test_tman_dependency_resolve()
