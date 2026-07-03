#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
from build.scripts import cmd_exec


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.tman_path: str
        self.pkg_src_root_dir: str
        self.output_path: str
        self.log_level: int


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--tman-path",
        type=str,
        required=True,
        help="Path to tman command itself",
    )
    parser.add_argument(
        "--pkg-src-root-dir",
        type=str,
        required=True,
        help="Path to app root folder",
    )
    parser.add_argument(
        "--output-path",
        type=str,
        required=False,
        help="Path of tman config file",
    )
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    if args.log_level > 0:
        print(f"> Packaging: {args.pkg_src_root_dir}")

    origin_wd = os.getcwd()
    returncode = 0
    output_text = ""

    try:
        os.chdir(args.pkg_src_root_dir)

        cmd = [args.tman_path, "package", "--output-path", args.output_path]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to package")

    except Exception as exc:
        returncode = 1
        print(exc)

    finally:
        if args.log_level > 0:
            print(output_text)

        os.chdir(origin_wd)
        sys.exit(returncode)
