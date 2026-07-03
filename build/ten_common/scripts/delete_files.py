#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import os
import stat
import shutil


def remove_readonly(func, path, excinfo):
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        func(path)
    else:
        raise excinfo[1]


def delete_files(files: list[str]):
    for file in files:
        if not os.path.exists(file):
            continue

        if os.path.isfile(file):
            os.remove(file)
        else:
            shutil.rmtree(file, onerror=remove_readonly)


if __name__ == "__main__":
    delete_files(sys.argv[1:])
