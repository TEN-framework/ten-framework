#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import os


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.target: str
        self.dependencies: list[str]
        self.depfile_path: str


def write_depfile(
    target: str, dependencies: list[str], depfile_path: str
) -> None:
    """
    Writes a Ninja-compatible depfile.

    Parameters:
    - target (str): The target file name.
    - dependencies (list of str): A list of dependency file names.
    - depfile_path (str): The path to the depfile to write.
    """
    # Convert each path to a format that is safe for depfiles.
    # Ninja requires paths with spaces to be escaped.
    safe_dependencies = [dep.replace(" ", "$ ") for dep in dependencies]

    # Format the content of the depfile.
    depfile_content = f"{target}: " + " ".join(safe_dependencies) + "\n"

    depfile_folder = os.path.dirname(depfile_path)

    # Check if the directory exists, and create it if it doesn't.
    if not os.path.exists(depfile_folder):
        os.makedirs(depfile_folder)

    # Write the content to the specified depfile.
    with open(depfile_path, "w", encoding="utf-8") as depfile:
        depfile.write(depfile_content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--dependencies", type=str, action="append", default=[])
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument("--depfile-path", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    write_depfile(args.target, args.dependencies, args.depfile_path)
