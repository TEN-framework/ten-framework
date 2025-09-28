#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import argparse
import subprocess
import sys
import os


def run_cmd(cmd: str, env: dict[str, str] | None = None) -> int:
    """Run a shell command."""
    if env is None:
        env = os.environ.copy()
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, check=True, env=env)
    return result.returncode


def run_cmd_build() -> int:
    """Build the application."""
    cmd = "npm install"

    rc = run_cmd(cmd)
    if rc != 0:
        return rc

    cmd = "npm run standalone-install"
    rc = run_cmd(cmd)
    if rc != 0:
        return rc

    cmd = "npm run build"
    rc = run_cmd(cmd)
    return rc


def main():
    parser = argparse.ArgumentParser(
        description="Run scripts based on manifest.json"
    )
    parser.add_argument("cmd", choices=["build"], help="Command to execute")

    args = parser.parse_args()

    # Handle the command based on platform.
    rc = 0

    if args.cmd == "build":
        rc = run_cmd_build()

    sys.exit(rc)


if __name__ == "__main__":
    main()
