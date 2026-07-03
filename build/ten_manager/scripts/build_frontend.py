#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
from build.scripts import cmd_exec


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.dir: str
        self.log_level: int


def main():
    parser = argparse.ArgumentParser(
        description="Build Designer Frontend using npm."
    )

    parser.add_argument(
        "--dir",
        type=str,
        required=True,
        help="Directory to run frontend build commands.",
    )
    parser.add_argument("--log-level", type=int, default=1)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    frontend_dir = args.dir
    log_level = args.log_level

    if not os.path.isdir(frontend_dir):
        sys.stderr.write(
            f"Error: The directory '{frontend_dir}' does not exist.\n"
        )
        sys.exit(1)

    try:
        install_cmd = "bun i --frozen-lockfile"
        returncode, output = cmd_exec.run_cmd_realtime(
            install_cmd, cwd=frontend_dir, log_level=log_level
        )
        if returncode != 0:
            sys.stderr.write("Error: 'bun i --frozen-lockfile' failed.\n")
            sys.stderr.write(output)
            sys.exit(returncode)

        # npm run lint
        lint_cmd = "npm run lint"
        returncode, output = cmd_exec.run_cmd_realtime(
            lint_cmd, cwd=frontend_dir, log_level=log_level
        )
        if returncode != 0:
            sys.stderr.write("Error: 'npm run lint' failed.\n")
            sys.stderr.write(output)
            sys.exit(returncode)

        # npm run build
        build_cmd = "npm run build"
        returncode, output = cmd_exec.run_cmd_realtime(
            build_cmd, cwd=frontend_dir, log_level=log_level
        )
        if returncode != 0:
            sys.stderr.write("Error: 'npm run build' failed.\n")
            sys.stderr.write(output)
            sys.exit(returncode)

        sys.exit(0)

    except Exception as exc:
        sys.stderr.write(f"Exception occurred: {exc}\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
