#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import os
from typing import Optional


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.libs: list[str]
        self.target_os: str


class LibFilter:
    def __init__(self, args: ArgumentInfo):
        self.args = args

    def is_candidate_libname(self, lib_path: str) -> bool:
        lib_name = os.path.basename(lib_path)
        if str(lib_name).endswith(".dll"):
            return False
        if str(lib_name).endswith("cmake_action_dummy_output_file"):
            return False
        return True

    def get_link_lib_name(self, lib_path: str) -> Optional[str]:
        lib_basename = os.path.basename(lib_path)
        if self.args.target_os == "win":
            return lib_basename

        basename = lib_basename
        if str(basename).startswith("lib"):
            basename = basename[3:]

        if str(basename).endswith(".dylib"):
            basename = basename[:-6]
        elif str(basename).endswith(".so"):
            basename = basename[:-3]
        elif str(basename).endswith(".a"):
            basename = basename[:-2]
        else:
            return None

        return basename

    def run(self) -> None:
        for lib in self.args.libs:
            if self.is_candidate_libname(lib):
                link_lib_name = self.get_link_lib_name(lib)
                if link_lib_name:
                    print(link_lib_name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--libs", type=str, action="append", default=[])
    parser.add_argument("--target-os", type=str, default="")

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    p = LibFilter(args)
    p.run()
