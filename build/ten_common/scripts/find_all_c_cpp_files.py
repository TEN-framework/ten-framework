#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import os


def main(argv):
    if len(argv) != 1:
        raise ValueError("Invalid parameter")

    path = os.path.abspath(argv[0])

    pattern = [
        ".h",
        ".hpp",
        ".c",
        ".cpp",
        ".cc",
    ]

    for dirpath, _, filenames in os.walk(path):
        for file in filenames:
            for p in pattern:
                if str(file).endswith(p):
                    print(os.path.join(path, dirpath, file))


if __name__ == "__main__":
    main(sys.argv[1:])
