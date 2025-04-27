#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import platform


def main(argv):
    if len(argv) != 0:
        raise ValueError("Invalid parameter")
    dir_name = os.path.dirname(os.path.abspath(__file__))

    dir_name = os.path.join(dir_name, "..", "..", "bin")
    if sys.platform == "win32":
        machine = platform.machine().lower()
        if machine in ["arm64", "aarch64"]:
            arch_folder = "arm64"
        elif machine in ["amd64", "x86_64"]:
            arch_folder = "x64"
        else:
            raise ValueError(f"Unsupported architecture: {machine}")

        dir_name = os.path.join(dir_name, "win", arch_folder, "ninja.exe")
        dir_name = os.path.abspath(dir_name).replace("\\", "/")
    elif sys.platform == "darwin":
        # The ninja for Mac is a binary that supports both x64 and arm64
        # architectures.
        dir_name = os.path.join(dir_name, "mac", "ninja")
        dir_name = os.path.abspath(dir_name)
    else:
        if os.uname().machine in ["arm64", "aarch64"]:
            dir_name = os.path.join(dir_name, "linux", "arm64", "ninja")
        else:
            dir_name = os.path.join(dir_name, "linux", "x64", "ninja")
        dir_name = os.path.abspath(dir_name)

    sys.stdout.write(dir_name)
    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
