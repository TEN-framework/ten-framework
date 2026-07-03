#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import subprocess
import os
from build.scripts import fs_utils, write_depfile


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.target_os: str
        self.target_arch: str
        self.asan_lib_dst_loc: str
        self.depfile: str


def detect_mac_asan_lib(arch: str) -> str:
    if arch == "x64":
        out, _ = subprocess.Popen(
            "clang -m64 -print-file-name=libclang_rt.asan_osx_dynamic.dylib",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    elif arch == "x86":
        out, _ = subprocess.Popen(
            "clang -m32 -print-file-name=libclang_rt.asan_osx_dynamic.dylib",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    elif arch == "arm64":
        out, _ = subprocess.Popen(
            "clang -m64 -print-file-name=libclang_rt.asan_osx_dynamic.dylib",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    else:
        raise ValueError(f"Unsupported architecture: {arch}")

    libasan_path = out.strip()

    if not libasan_path:
        raise FileNotFoundError(
            "Failed to find libclang_rt.asan_osx_dynamic.dylib"
        )

    real_libasan_path = os.path.realpath(libasan_path)

    return real_libasan_path


def detect_linux_asan_lib(arch: str) -> str:
    if arch in ["x64", "arm64"]:
        out, _ = subprocess.Popen(
            "gcc -print-file-name=libasan.so",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    elif arch == "x86":
        out, _ = subprocess.Popen(
            "gcc -m32 -print-file-name=libasan.so",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    else:
        raise ValueError(f"Unsupported architecture: {arch}")

    libasan_path = out.strip()

    if not libasan_path:
        raise FileNotFoundError("Failed to find libasan.so")

    real_libasan_path = os.path.realpath(libasan_path)

    return real_libasan_path


# Generally speaking, this function should not need to be called because Clang's
# default ASan mechanism is static linking.
def detect_linux_clang_asan_lib(arch: str) -> str:
    if arch == "x64":
        out, _ = subprocess.Popen(
            "clang -print-file-name=libclang_rt.asan-x86_64.so",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    elif arch == "arm64":
        out, _ = subprocess.Popen(
            "clang -print-file-name=libclang_rt.asan-aarch64.so",
            shell=True,
            stdout=subprocess.PIPE,
            encoding="utf-8",
        ).communicate()
    else:
        raise ValueError(f"Unsupported architecture: {arch}")

    libasan_path = out.strip()

    if not libasan_path:
        raise FileNotFoundError("Failed to find libclang_rt.asan-x86_64.so")

    real_libasan_path = os.path.realpath(libasan_path)

    return real_libasan_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--target-os", type=str)
    parser.add_argument("--target-arch", type=str)
    parser.add_argument("--asan-lib-dst-loc", type=str)
    parser.add_argument("--depfile", type=str)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    if args.target_os == "mac":
        asan_lib_src_loc = detect_mac_asan_lib(args.target_arch)
    elif args.target_os == "linux":
        asan_lib_src_loc = detect_linux_asan_lib(args.target_arch)
    elif args.target_os == "linux_clang":
        asan_lib_src_loc = detect_linux_clang_asan_lib(args.target_arch)
    else:
        raise ValueError("Unsupported OS.")

    fs_utils.copy(asan_lib_src_loc, args.asan_lib_dst_loc)

    write_depfile.write_depfile(
        os.path.basename(args.asan_lib_dst_loc),
        [asan_lib_src_loc],
        args.depfile,
    )
