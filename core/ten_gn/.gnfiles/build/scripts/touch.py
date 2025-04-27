#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import argparse


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.path: str


def touch(path: str):
    # Create all the parent folders.
    parent_dir = os.path.dirname(path)
    if parent_dir and not os.path.exists(parent_dir):
        os.makedirs(parent_dir, exist_ok=True)

    with open(path, "a", encoding="utf-8"):
        try:
            os.utime(path, follow_symlinks=False)
        except Exception:
            try:
                # If follow_symlinks parameter is not supported, fall back to
                # default behavior
                os.utime(path)
            except Exception:
                sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=(
            "Create a file if it does not exist and update its access and "
            "modification times."
        )
    )
    parser.add_argument("path", type=str, help="The path of the file to touch.")

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    touch(args.path)
