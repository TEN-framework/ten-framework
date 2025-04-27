#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import os
import sys
import glob
import json


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.dir: list[str]
        self.dir_base: list[str] = []
        self.recursive: bool
        self.only_output_file: bool


def glob_file(
    dirs: list[str], dir_bases: list[str], recursive: bool, only_file: bool
) -> None:
    output = []

    for index, item in enumerate(dirs):
        dir_base = dir_bases[index] if index < len(dir_bases) else ""
        for v in glob.glob(item, recursive=recursive):
            if only_file:
                if not os.path.isfile(v):
                    continue

            relative_path = os.path.relpath(v, dir_base) if dir_base else ""

            output.append(
                {
                    "path": v.replace("\\", "/"),
                    "relative_path": relative_path,
                }
            )

    json.dump(output, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", type=str, action="append", default=[])
    parser.add_argument("--dir-base", type=str, action="append", default=[])
    parser.add_argument("--recursive", action="store_true")
    # Skip directory in the output.
    parser.add_argument("--only-output-file", action="store_true")

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    glob_file(args.dir, args.dir_base, args.recursive, args.only_output_file)

    sys.exit(0)
