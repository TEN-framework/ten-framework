#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import tomlkit
from tomlkit.items import Table, InlineTable
import argparse


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.cargo_toml_path: str


def sort_and_inline_dependencies(file_path):
    # Read the original `Cargo.toml` file.
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Parse the content using tomlkit.
    data = tomlkit.parse(content)

    # Define the parts which need to be sorted.
    sections = ["dependencies", "dev-dependencies", "build-dependencies"]

    for section in sections:
        if section in data:
            original_table = data[section]
            if isinstance(original_table, Table):
                # Extract key-value pairs and sort them by key
                # (case-insensitive).
                sorted_items = sorted(
                    original_table.items(), key=lambda x: x[0].lower()
                )

                # Create a new table to store the sorted dependencies.
                new_table = tomlkit.table()
                for key, value in sorted_items:
                    if isinstance(value, str):
                        # Convert dependencies in string format into inline
                        # tables containing only the version.
                        inline = tomlkit.inline_table()
                        inline.add("version", value)
                        new_table[key] = inline
                    elif isinstance(value, InlineTable):
                        # Keep dependencies that are already in inline table
                        # format unchanged.
                        new_table[key] = value
                    elif isinstance(value, Table):
                        # Handle dependencies in standard table format and
                        # convert them into inline tables.
                        inline = tomlkit.inline_table()
                        for sub_key, sub_value in value.items():
                            inline.add(sub_key, sub_value)
                        new_table[key] = inline

                # Replace the original dependency table with the newly sorted
                # table.
                data[section] = new_table

    # Write the sorted and converted content back to the file, preserving the
    # original format and comments.
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(tomlkit.dumps(data))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Sort and inline Cargo.toml dependencies."
    )

    parser.add_argument(
        "cargo_toml_path",
        type=str,
        help="Path to the Cargo.toml file",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    sort_and_inline_dependencies(args.cargo_toml_path)
    print("Dependencies sorted and converted to inline table format!")
