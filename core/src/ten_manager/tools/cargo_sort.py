#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import toml


def sort_dependencies(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        data = toml.load(f)

    for section in ["dependencies", "dev-dependencies", "build-dependencies"]:
        if section in data:
            data[section] = dict(sorted(data[section].items()))

    with open(file_path, "w", encoding="utf-8") as f:
        toml.dump(data, f)


if __name__ == "__main__":
    sort_dependencies("Cargo.toml")
    print("Dependencies sorted!")
