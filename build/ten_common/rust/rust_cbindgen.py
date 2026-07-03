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

        self.project_path: str
        self.log_level: int
        self.config_file: str
        self.output_file: str


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--project-path", type=str, required=True)
    parser.add_argument("--config-file", type=str, required=False)
    parser.add_argument("--output-file", type=str, required=True)
    parser.add_argument("--log-level", type=int, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    origin_wd = os.getcwd()

    returncode = 0
    try:
        os.chdir(args.project_path)

        cmd = [
            "cbindgen",
            "--config",
            args.config_file,
            "--output",
            args.output_file,
        ]

        returncode, logs = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError(f"Failed to gen header files for rust: {logs}")

    except Exception as exc:
        returncode = 1
        print(exc)

    finally:
        os.chdir(origin_wd)
        sys.exit(-1 if returncode != 0 else 0)
