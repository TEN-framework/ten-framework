#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from build.scripts import fs_utils


def main(src_file: str, dst_file: str) -> None:
    # Get absolute paths.
    src_abs = os.path.abspath(src_file)
    dst_abs = os.path.abspath(dst_file)

    # Create destination directory if needed.
    dst_dir = os.path.dirname(dst_abs)
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    # Remove existing destination if it exists.
    if os.path.exists(dst_abs):
        if os.path.islink(dst_abs):
            os.unlink(dst_abs)
        elif os.path.isdir(dst_abs):
            fs_utils.remove_tree(dst_abs)
        else:
            os.remove(dst_abs)

    # Get relative path from destination to source.
    rel_path = os.path.relpath(src_abs, dst_dir)

    # Create symlink.
    os.symlink(rel_path, dst_abs)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: create_symlink.py <source_file> <destination_file>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
