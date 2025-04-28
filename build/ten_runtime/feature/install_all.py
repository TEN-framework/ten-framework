#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
from build.scripts import cmd_exec, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.tman_path: str
        self.pkg_src_root_dir: str
        self.tg_timestamp_proxy_file: str
        self.build_type: str
        self.config_file: str
        self.log_level: int
        self.assume_yes: bool


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
        "--tg-timestamp-proxy-file",
        type=str,
        required=True,
        help="Path to the stamp file",
    )
    parser.add_argument(
        "--build-type",
        type=str,
        required=False,
        help="'debug', 'release'",
    )
    parser.add_argument(
        "--config-file",
        type=str,
        required=False,
        help="Path of tman config file",
    )
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )
    parser.add_argument("--assume-yes", type=bool, default=False)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    if args.log_level > 0:
        print(f"> Install_all: {args.pkg_src_root_dir}")

    origin_wd = os.getcwd()
    returncode = 0
    output_text = ""

    try:
        os.chdir(args.pkg_src_root_dir)

        cmd = [args.tman_path]

        if args.config_file is not None:
            cmd += ["--config-file=" + args.config_file]

        if args.assume_yes:
            cmd += ["--yes"]

        cmd += ["install"]

        if args.build_type is not None:
            cmd += ["--build-type=" + args.build_type]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to install_all")
        else:
            # install_all successfully, create the stamp file to represent this
            # operation is success.
            timestamp_proxy.touch_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )

    except Exception as exc:
        # Failed to install_all, delete the stamp file to represent this
        # operation is failed.
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        returncode = 1
        print(exc)

    finally:
        if args.log_level > 0:
            print(output_text)

        os.chdir(origin_wd)
        sys.exit(returncode)
