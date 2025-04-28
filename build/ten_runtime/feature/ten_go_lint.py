#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import os
import subprocess
import sys
from build.scripts import timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.go: str
        self.lint_dir: str
        self.tg_timestamp_proxy_file: str


def run_cmd(cmd: list[str] | str, cwd: str) -> int:
    my_cmd = cmd
    set_shell = True

    if isinstance(cmd, list):
        if sys.platform == "win32":
            if cmd[0] != "cmd":
                my_cmd = ["cmd", "/c"] + cmd
        else:
            set_shell = False

    if isinstance(cmd, str):
        if sys.platform == "win32" and cmd[:3] != "cmd":
            my_cmd = "cmd /c " + cmd

    child = subprocess.Popen(
        my_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        shell=set_shell,
        bufsize=0,
        cwd=cwd,
    )

    while child.poll() is None:
        line = ""
        if child.stdout:
            try:
                line = child.stdout.readline()
            except UnicodeDecodeError:
                line = child.stdout.readline().encode("gbk")

        if line != "":
            sys.stdout.flush()
            sys.stdout.write("{}\n".format(line.rstrip()))
            sys.stdout.flush()

    return child.returncode


def detect_go_module_path(path: str, level: int = 3) -> str:
    if level == 0:
        return ""

    if os.path.exists(os.path.join(path, "go.mod")):
        return path

    return detect_go_module_path(os.path.dirname(path), level - 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--go", type=str, required=True)
    parser.add_argument("--lint-dir", type=str, required=True)
    parser.add_argument("--tg-timestamp-proxy-file", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    if not os.path.isdir(args.lint_dir):
        print("The --lint-dir should be a directory contains go files.")
        sys.exit(-1)

    module_path = detect_go_module_path(args.lint_dir)
    if module_path == "":
        print("Failed to find go.mod.")
        sys.exit(-1)

    returncode = 0
    cmd = [args.go, "vet", args.lint_dir]

    try:
        returncode = run_cmd(cmd, module_path)
        if returncode != 0:
            raise RuntimeError("Failed to go lint.")

        # Success to build the app, update the stamp file to represent this
        # fact.
        timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)

    except Exception as exc:
        returncode = 1
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        print("Failed to run go lint:")
        print(f"\t{exc}")

    finally:
        sys.exit(returncode)
