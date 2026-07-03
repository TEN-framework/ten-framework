#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from .utils import cmd_exec


def get_installed_extensions_count(app_dir: str):
    extension_dir = os.path.join(app_dir, "ten_packages/extension/")
    extensions = os.listdir(extension_dir)

    return len(extensions)


def normalize_path(path):
    if path.startswith("\\\\?\\"):
        return path[4:]
    return path


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
        ],
        cwd=app_dir,
    )
    if returncode != 0:
        print(output_text)
        assert False

    installed_count = get_installed_extensions_count(app_dir)
    assert (
        installed_count == 2
    ), f"Expected 2 extensions, found {installed_count}."

    ext_c_path = os.path.join(app_dir, "ten_packages/extension/ext_c")
    local_c_path = os.path.join(app_dir, "local_c")

    # Check if `ext_c_path` exists.
    assert os.path.exists(
        ext_c_path
    ), f"Expected ext_c path '{ext_c_path}' does not exist."

    # Check if `ext_c_path` is a symbolic link.
    is_symlink = os.path.islink(ext_c_path)
    assert is_symlink, f"Expected '{ext_c_path}' to be a symbolic link."

    # Retrieve the target path of the symbolic link.
    try:
        symlink_target = os.readlink(ext_c_path)
    except OSError as e:
        assert False, f"Failed to read symbolic link '{ext_c_path}': {e}"

    # If `symlink_target` is a relative path, it needs to be converted to an
    # absolute path.
    if not os.path.isabs(symlink_target):
        symlink_target = os.path.normpath(
            os.path.join(os.path.dirname(ext_c_path), symlink_target)
        )

    # Normalize the path to eliminate any redundancies (e.g., `./`).
    symlink_target = os.path.normpath(symlink_target)
    expected_target = os.path.normpath(local_c_path)

    # On Windows, `os.readlink` may return paths with backslashes, so it's
    # necessary to standardize the path separators.
    symlink_target = os.path.abspath(symlink_target)
    expected_target = os.path.abspath(expected_target)

    # Normalize paths by removing '\\?\' prefix if present
    symlink_target = normalize_path(symlink_target)
    expected_target = normalize_path(expected_target)

    assert symlink_target == expected_target, (
        f"Symbolic link target mismatch for '{ext_c_path}'. "
        f"Expected: '{expected_target}', Found: '{symlink_target}'."
    )

    print(
        f"Symbolic link '{ext_c_path}' correctly points to '{symlink_target}'."
    )


if __name__ == "__main__":
    test_tman_dependency_resolve()
