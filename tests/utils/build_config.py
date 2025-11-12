#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
class BuildConfig:
    def __init__(
        self,
        target_os,
        target_cpu,
        target_build,
        is_clang,
        enable_sanitizer,
        vs_version,
        ten_enable_integration_tests_prebuilt,
        ten_enable_tests_cleanup,
    ):
        self.target_os = target_os
        self.target_cpu = target_cpu
        self.target_build = target_build
        self.is_clang = is_clang
        self.enable_sanitizer = enable_sanitizer
        self.vs_version = vs_version
        self.ten_enable_integration_tests_prebuilt = (
            ten_enable_integration_tests_prebuilt
        )
        self.ten_enable_tests_cleanup = ten_enable_tests_cleanup


def parse_build_config(file_path: str) -> BuildConfig:
    target_os = None
    target_cpu = None
    is_debug = None
    is_clang = None
    enable_sanitizer = None
    vs_version = None
    ten_enable_integration_tests_prebuilt = None
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
            elif line.startswith("enable_sanitizer"):
                enable_sanitizer = line.split("=")[1].strip().lower() == "true"
            elif line.startswith("vs_version"):
                vs_version = line.split("=")[1].strip().strip('"')
            elif line.startswith("ten_enable_integration_tests_prebuilt"):
                ten_enable_integration_tests_prebuilt = (
                    line.split("=")[1].strip().lower() == "true"
                )
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
        enable_sanitizer,
        vs_version,
        ten_enable_integration_tests_prebuilt,
        ten_enable_tests_cleanup,
    )
