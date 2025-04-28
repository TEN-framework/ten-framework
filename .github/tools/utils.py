#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import subprocess
import sys
import time


def run_cmd(cmd):
    # print(cmd)

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
    )

    output = ""
    while child.poll() is None:
        line = ""
        if child.stdout:
            try:
                line = child.stdout.readline()
            except UnicodeDecodeError:
                line = child.stdout.readline().encode("gbk")

        if line != "":
            output += str(line)

            sys.stdout.flush()
            sys.stdout.write("{}\n".format(line.rstrip()))
            sys.stdout.flush()

    return child.returncode, output


def run_cmd_with_retry(cmd: list[str], cnt: int = 10):
    for i in range(1, cnt):
        rc, output = run_cmd(cmd)
        if rc == 0:
            return rc, output
        else:
            time.sleep(1)
    assert False
    return []
