#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import zipfile
import sys
import os


def unzip_file(src_path: str, dest_path: str) -> int:
    if not os.path.exists(src_path):
        return -1

    src_dir = os.path.dirname(src_path)
    dst_dir = src_dir if dest_path == "" else dest_path
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    zip_ref = zipfile.ZipFile(src_path, "r")
    zip_ref.extractall(dst_dir)
    zip_ref.close()

    return 0


def main(argv) -> int:
    if len(argv) < 2:
        return -1
    dest_path = "" if len(argv) < 3 else argv[2]
    return unzip_file(argv[0], dest_path)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
