#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import json
import os
import sys
from ten_common.scripts import delete_files


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.tman_config_file_path: str
        self.registry_path: str
        self.log_level: int


def create_tman_config_json(
    tman_config_file_path: str, registry_path: str
) -> None:
    data = {
        "registry": {"default": {"index": registry_path}},
        "enable_package_cache": False,
    }

    with open(tman_config_file_path, "w", encoding="utf-8") as file:
        json.dump(data, file, indent=2)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--tman-config-file-path", type=str, required=True)
    parser.add_argument("--registry-path", type=str, required=True)
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    if args.log_level > 0:
        print(f"> Create config.json: {args.tman_config_file_path}")

    origin_wd = os.getcwd()
    returncode = 0

    try:
        create_tman_config_json(args.tman_config_file_path, args.registry_path)

        if args.log_level > 0:
            print("Create config.json success")

    except Exception as e:
        returncode = 1
        delete_files.delete_files(
            [os.path.join(f"{args.tman_config_file_path}")]
        )
        print(f"Create config.json failed: {repr(e)}")

    finally:
        os.chdir(origin_wd)
        sys.exit(returncode)
