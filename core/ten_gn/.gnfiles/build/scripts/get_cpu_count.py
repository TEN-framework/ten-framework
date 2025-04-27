#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import multiprocessing
import sys


def main():
    try:
        cpu_count = multiprocessing.cpu_count()
    except Exception:
        cpu_count = 1

    print(cpu_count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
