#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys


def find_mingw_gcc() -> str:
    """
    Search for MinGW gcc in PATH environment variable.
    Looks for directories containing "mingw" and ending with "bin",
    then verifies the presence of gcc.exe.
    Returns the full path to gcc.exe if found, empty string otherwise.
    """
    if sys.platform != "win32":
        return ""

    path_env = os.environ.get("PATH", "")
    if not path_env:
        return ""

    for path_dir in path_env.split(os.pathsep):
        path_dir = path_dir.strip()
        if not path_dir:
            continue

        # Check if this is a MinGW bin directory:
        # 1. Path contains "mingw" (case-insensitive)
        # 2. Path ends with "bin"
        path_dir_lower = path_dir.lower()
        if "mingw" not in path_dir_lower or not path_dir_lower.endswith("bin"):
            continue

        # Verify it's a real MinGW directory by checking for gcc.exe
        gcc_path = os.path.join(path_dir, "gcc.exe")
        if os.path.isfile(gcc_path):
            return gcc_path

    return ""


def find_mingw_gxx() -> str:
    """
    Search for MinGW g++ in PATH environment variable.
    Returns the full path to g++.exe if found, empty string otherwise.
    """
    gcc_path = find_mingw_gcc()
    if gcc_path:
        return gcc_path.replace("gcc.exe", "g++.exe")
    return ""


class BuildConfig:
    def __init__(
        self,
        target_os,
        target_cpu,
        target_build,
        is_clang,
        is_mingw,
        enable_sanitizer,
        vs_version,
        ten_enable_tests_cleanup,
    ):
        self.target_os = target_os
        self.target_cpu = target_cpu
        self.target_build = target_build
        self.is_clang = is_clang
        self.is_mingw = is_mingw
        self.enable_sanitizer = enable_sanitizer
        self.vs_version = vs_version
        self.ten_enable_tests_cleanup = ten_enable_tests_cleanup


def parse_build_config(file_path: str) -> BuildConfig:
    target_os = None
    target_cpu = None
    is_debug = None
    is_clang = None
    is_mingw = None
    enable_sanitizer = None
    vs_version = None
    ten_enable_tests_cleanup = None

    with open(file_path, "r", encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if line.startswith("target_os"):
                target_os = line.split("=")[1].strip().strip('"')
            elif line.startswith("target_cpu"):
                target_cpu = line.split("=")[1].strip().strip('"')
            elif line.startswith("is_debug"):
                is_debug = line.split("=")[1].strip().lower() == "true"
            elif line.startswith("is_clang"):
                is_clang = line.split("=")[1].strip().lower() == "true"
            elif line.startswith("is_mingw"):
                is_mingw = line.split("=")[1].strip().lower() == "true"
            elif line.startswith("enable_sanitizer"):
                enable_sanitizer = line.split("=")[1].strip().lower() == "true"
            elif line.startswith("vs_version"):
                vs_version = line.split("=")[1].strip().strip('"')
            elif line.startswith("ten_enable_tests_cleanup"):
                ten_enable_tests_cleanup = (
                    line.split("=")[1].strip().lower() == "true"
                )

    target_build = "debug" if is_debug else "release"

    return BuildConfig(
        target_os,
        target_cpu,
        target_build,
        is_clang,
        is_mingw,
        enable_sanitizer,
        vs_version,
        ten_enable_tests_cleanup,
    )
