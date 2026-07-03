#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import fnmatch


def main(argv):
    if len(argv) < 1:
        raise ValueError("Invalid parameter")
    re_str = argv[0]
    argv = argv[1:]
    for v in fnmatch.filter(argv, re_str):
        sys.stdout.write(v.replace("\\", "/") + "\n")
    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
