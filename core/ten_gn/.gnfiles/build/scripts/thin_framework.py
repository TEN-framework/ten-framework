#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
from build.scripts import cmd_exec


def run_system(argv):
    cmd = "lipo -archs " + argv[0]
    status, output = cmd_exec.get_cmd_output(cmd)
    if status != 0:
        sys.stderr.write(output)
        sys.stderr.write("\n")
        sys.stderr.flush()
    else:
        if (len(str(output).split(" ")) > 1) and set(
            str(output).split(" ")
        ).issubset(["x86_64", "armv7", "arm64"]):
            cmd = "lipo -thin  {} {} -o {}".format(argv[1], argv[0], argv[0])
            status, output = cmd_exec.get_cmd_output(cmd)
            if status != 0:
                sys.stderr.write(output)
                sys.stderr.write("\n")
                sys.stderr.flush()
        else:
            print(output)
    sys.exit(-1 if status != 0 else 0)


if __name__ == "__main__":
    run_system(sys.argv[1:])
