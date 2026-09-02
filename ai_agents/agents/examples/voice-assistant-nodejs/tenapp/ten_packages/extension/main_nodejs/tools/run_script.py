#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import argparse
import platform
import subprocess
import sys
import os

# On Windows npm ships as a batch script (`npm.cmd`). CreateProcess does not
# apply PATHEXT resolution, so the extension has to be spelled out.
NPM = "npm.cmd" if platform.system().lower() == "windows" else "npm"


def run_cmd(cmd: list[str], env: dict[str, str] | None = None) -> int:
    """Run a command without spawning a shell."""
    if env is None:
        env = os.environ.copy()
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, check=True, env=env)
    return result.returncode


def run_cmd_build() -> int:
    """Build the application."""
    cmd = [NPM, "install"]

    rc = run_cmd(cmd)
    if rc != 0:
        return rc

    cmd = [NPM, "run", "standalone-install"]
    rc = run_cmd(cmd)
    if rc != 0:
        return rc

    cmd = [NPM, "run", "build"]
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
