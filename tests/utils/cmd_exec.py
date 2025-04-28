#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import os
import subprocess
from typing import Optional


# `psutil` is not a builtin module, so use it if it exists.
try:
    import psutil

    has_psutil = True
except ImportError:
    has_psutil = False


def get_cmd_output(cmd: str, log_level: int = 0) -> tuple[int, str]:
    """Return (status, output) of executing cmd in a shell."""

    if sys.platform != "win32":
        if cmd.endswith(";"):
            cmd = "{ " + cmd + " }"
        else:
            cmd = "{ " + cmd + "; }"
    else:
        # Leaves the item unchanged if it starts with "http", or replaces all
        # forward slashes (/) with backslashes (\) in other items, and the items
        # are then joined back into a single string with spaces.
        cmd = " ".join(
            [
                (
                    item
                    if item.startswith("http") or item.startswith("/")
                    else item.replace("/", "\\")
                )
                for item in cmd.split()
            ]
        )

    if log_level > 1:
        print(f"> {cmd}")

    # Use UTF-8 encoding with the `replace` option to avoid decode errors when
    # Python handling the `output_text`.
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output_text = ""

    # Read all output at once.
    output_text, _ = process.communicate()

    returncode = process.returncode
    if returncode is None:
        returncode = 0

    # According to the implementation of 'os::popen()', the 'returncode' will
    # be shifted left to match old behavior if the system is not Windows NT.
    # Refer to '_wrap_close::close()' in os.py.
    if os.name != "nt" and returncode >= 1 << 8:
        returncode = returncode >> 8

    if output_text[-1:] == "\n":
        output_text = output_text[:-1]

    return returncode, output_text


def run_cmd(argv: list[str], log_level: int = 0) -> tuple[int, str]:
    cmd = " ".join(argv)
    returncode, output_text = get_cmd_output(cmd, log_level)
    if returncode != 0:
        sys.stderr.write(output_text)
        sys.stderr.write("\n")
        sys.stderr.flush()
    return returncode, output_text


def run_cmd_realtime(
    cmd,
    cwd: Optional[str] = None,
    env: Optional[dict[str, str]] = None,
    log_level: int = 0,
):
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
        text=True,
        shell=set_shell,
        bufsize=0,
        env=env,
        cwd=cwd,
    )

    console_log = ""
    pid = child.pid
    process_name = ""

    if has_psutil:
        try:
            process = psutil.Process(pid)
            process_name = process.name()
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    line = ""

    try:
        while child.poll() is None:
            if child.stdout:
                try:
                    line = child.stdout.readline()
                except UnicodeDecodeError:
                    line = child.stdout.readline().encode("gbk").decode("gbk")

            if line != "":
                console_log += line
                if log_level > 0:
                    sys.stdout.flush()
                    if log_level == 3:
                        sys.stdout.write(
                            "{}: {}{} > {}\n".format(
                                cmd,
                                process_name if process_name else "",
                                f"({pid})",
                                line.rstrip(),
                            )
                        )
                    elif log_level == 2:
                        sys.stdout.write(
                            "{}{} > {}\n".format(
                                process_name if process_name else "",
                                f"({pid})",
                                line.rstrip(),
                            )
                        )

                    sys.stdout.flush()
    except UnicodeDecodeError:
        pass
    except Exception as e:
        sys.stdout.write(
            "{}{} > {}\n".format(
                process_name if process_name else "", f"({pid})", line.rstrip()
            )
        )
        print(repr(e))
        raise e

    return child.returncode, console_log
