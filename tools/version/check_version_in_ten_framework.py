#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
from common import get_latest_git_tag


def check_preserved_metadata_version_of_ten_runtime(
    repo_base_dir: str, git_version: str
) -> bool:
    c_preserved_metadata_file_src_file = os.path.join(
        repo_base_dir,
        "core",
        "include_internal",
        "ten_runtime",
        "common",
        "version.h",
    )

    if os.path.exists(c_preserved_metadata_file_src_file):
        with open(
            c_preserved_metadata_file_src_file, "r", encoding="utf-8"
        ) as f:
            content = f.read()

        if f'VERSION "{git_version}"' in content:
            return True

    return False


def check_preserved_metadata_version_of_ten_runtime_go_binding(
    repo_base_dir: str, git_version: str
) -> bool:
    c_preserved_metadata_file_src_file = os.path.join(
        repo_base_dir,
        "core",
        "include_internal",
        "ten_runtime",
        "binding",
        "go",
        "internal",
        "version.h",
    )

    if os.path.exists(c_preserved_metadata_file_src_file):
        with open(
            c_preserved_metadata_file_src_file, "r", encoding="utf-8"
        ) as f:
            content = f.read()

        if f'VERSION "{git_version}"' in content:
            return True

    return False


def check_preserved_metadata_version_of_ten_runtime_nodejs_binding(
    repo_base_dir: str, git_version: str
) -> bool:
    c_preserved_metadata_file_src_file = os.path.join(
        repo_base_dir,
        "core",
        "include_internal",
        "ten_runtime",
        "binding",
        "nodejs",
        "common",
        "version.h",
    )

    if os.path.exists(c_preserved_metadata_file_src_file):
        with open(
            c_preserved_metadata_file_src_file, "r", encoding="utf-8"
        ) as f:
            content = f.read()

        if f'VERSION "{git_version}"' in content:
            return True

    return False


def check_preserved_metadata_version_of_ten_runtime_python_binding(
    repo_base_dir: str, git_version: str
) -> bool:
    c_preserved_metadata_file_src_file = os.path.join(
        repo_base_dir,
        "core",
        "include_internal",
        "ten_runtime",
        "binding",
        "python",
        "common",
        "version.h",
    )

    if os.path.exists(c_preserved_metadata_file_src_file):
        with open(
            c_preserved_metadata_file_src_file, "r", encoding="utf-8"
        ) as f:
            content = f.read()

        if f'VERSION "{git_version}"' in content:
            return True

    return False


def check_version_of_tman(repo_base_dir: str, git_version: str) -> bool:
    tman_version_src_file = os.path.join(
        repo_base_dir,
        "core",
        "src",
        "ten_manager",
        "src",
        "version.rs",
    )

    if os.path.exists(tman_version_src_file):
        with open(tman_version_src_file, "r", encoding="utf-8") as f:
            content = f.read()

        if f'VERSION: &str = "{git_version}"' in content:
            return True

    return False


def check_version_of_system_packages(
    repo_base_dir: str, git_version: str
) -> bool:
    manifest_files = [
        # ten_runtime
        os.path.join(
            repo_base_dir,
            "core",
            "src",
            "ten_runtime",
            "manifest.json",
        ),
        # ten_runtime_go
        os.path.join(
            repo_base_dir,
            "core",
            "src",
            "ten_runtime",
            "binding",
            "go",
            "manifest.json",
        ),
        # ten_runtime_python
        os.path.join(
            repo_base_dir,
            "core",
            "src",
            "ten_runtime",
            "binding",
            "python",
            "manifest.json",
        ),
        # ten_runtime_nodejs
        os.path.join(
            repo_base_dir,
            "core",
            "src",
            "ten_runtime",
            "binding",
            "nodejs",
            "manifest.json",
        ),
    ]

    for manifest_file in manifest_files:
        if not os.path.exists(manifest_file):
            return False

        with open(manifest_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        if data.get("version") != git_version:
            return False

    return True


def check_various_versions(repo_base_dir: str, git_version: str) -> bool:
    if not check_preserved_metadata_version_of_ten_runtime(
        repo_base_dir, git_version
    ):
        print("ten_runtime preserved_metadata version does not match.")
        return False

    if not check_preserved_metadata_version_of_ten_runtime_go_binding(
        repo_base_dir, git_version
    ):
        print("ten_runtime_go preserved_metadata version does not match.")
        return False

    if not check_preserved_metadata_version_of_ten_runtime_nodejs_binding(
        repo_base_dir, git_version
    ):
        print("ten_runtime_nodejs preserved_metadata version does not match.")
        return False

    if not check_preserved_metadata_version_of_ten_runtime_python_binding(
        repo_base_dir, git_version
    ):
        print("ten_runtime_python preserved_metadata version does not match.")
        return False

    if not check_version_of_tman(repo_base_dir, git_version):
        print("ten_manager version does not match.")
        return False

    if not check_version_of_system_packages(repo_base_dir, git_version):
        print("ten system package versions do not match.")
        return False

    return True


if __name__ == "__main__":
    repo_base_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..")
    )

    # Change to the correct directory to get the correct git tag.
    os.chdir(repo_base_dir)

    # Get the latest Git tag, and optionally strip leading 'v' if present.
    git_version = get_latest_git_tag()
    git_version = git_version.lstrip("v")

    if check_various_versions(repo_base_dir, git_version):
        print("All versions match.")
    else:
        print("Versions do not match.")
        exit(1)
