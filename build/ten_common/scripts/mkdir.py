#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import os
import argparse
from build.scripts import log


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.path: str


def mkdir(path: str):
    try:
        os.makedirs(path, exist_ok=True)
        log.info(f"Create successfully: {path}")
    except Exception as e:
        log.error(f"Failed to create directory: {path}\n: {e}")
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Create a directory at the specified path."
    )
    parser.add_argument("path", type=str, help="The path of the file to touch.")

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    mkdir(args.path)
