#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
from typing import Optional
from build.scripts import cmd_exec, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.project_path: str
        self.target_path: str
        self.target: str
        self.log_level: int
        self.tg_timestamp_proxy_file: Optional[str] = None


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--project-path", type=str, required=True)
    parser.add_argument("--target-path", type=str, required=True)
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument("--log-level", type=int, required=True)
    parser.add_argument(
        "--tg-timestamp-proxy-file", type=str, default="", required=False
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    origin_wd = os.getcwd()

    returncode = 0
    try:
        os.chdir(args.project_path)

        cmd = [
            "cargo",
            "clean",
            "--target-dir",
            args.target_path,
            "--target",
            args.target,
        ]

        returncode, logs = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError(
                f"Failed to run `cargo clean` on {args.project_path}: {logs}"
            )
        else:
            print(logs)

        # Success to clean the rust project, update the stamp file to represent
        # this fact.
        timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)

    except Exception as exc:
        returncode = 1
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        print(exc)

    finally:
        os.chdir(origin_wd)
        sys.exit(-1 if returncode != 0 else 0)
