#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
import shutil
from typing import Optional
from dotenv import dotenv_values
from io import StringIO
from build.scripts import cmd_exec


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.library: list[str]
        self.target_path: str
        self.target: str
        self.output: str
        self.env_file: str
        self.log_level: int


def ar_extract(library: str, log_level: int) -> None:
    # No --output option on macos.
    cmd = ["ar", "-x", library]
    returncode, output = cmd_exec.run_cmd(cmd, log_level)
    if returncode:
        print(f"Failed to extract static library: {output}")
        raise RuntimeError("Failed to extract static library.")


def ar_create(target_library: str, target_path: str, log_level: int) -> None:
    cmd = ["ar", "-rcs", target_library, f"{target_path}/*.o"]
    returncode, output = cmd_exec.run_cmd(cmd, log_level)
    if returncode:
        print(f"Failed to create static library: {output}")
        raise RuntimeError("Failed to create static library.")


def read_path_from_env_file(env_file: str) -> Optional[str]:
    with open(env_file, "rb") as f:
        for line in f:
            # NUL character.
            lines = line.split(b"\x00")
            lines = [data.decode("utf-8") for data in lines]
            cfg = StringIO("\n".join(lines))
            configs = dotenv_values(stream=cfg)
            if "Path" in configs:
                return configs["Path"]

    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--library", type=str, action="append", default=[])
    parser.add_argument("--target-path", type=str, required=True)
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument("--output", type=str, required=False)
    parser.add_argument("--env-file", type=str, required=False)
    parser.add_argument("--log-level", type=int, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    is_windows = sys.platform == "win32"

    # The environment is required on Windows, we need to find lib.exe based on
    # the environment.
    if is_windows:
        if arg_info.env_file is None:
            print("The environment file is required on Windows.")
            sys.exit(-1)

        if not os.path.exists(arg_info.env_file):
            print(f"The environment file {arg_info.env_file} does not exist.")
            sys.exit(-1)

        path = read_path_from_env_file(arg_info.env_file)
        if not path:
            print("Failed to read Path from the environment file.")
            sys.exit(-1)
        os.environ["Path"] = path

    target_library = ""
    if arg_info.output is None:
        target_library = os.path.join(
            arg_info.target_path, os.path.basename(arg_info.library[0])
        )
    else:
        target_library = os.path.join(arg_info.target_path, arg_info.output)

    if is_windows:
        cmd = [
            "lib",
            "/OUT:" + target_library,
            "/NOLOGO",
            "/MACHINE:" + arg_info.target,
        ]
        cmd.extend(arg_info.library)
        returncode, output = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            print(f"Failed to combine static library: {output}")
            sys.exit(-1)
    else:
        if not os.path.exists(target_library):
            shutil.copy(arg_info.library[0], target_library)

        origin_wd = os.getcwd()

        tmp_output = os.path.join(arg_info.target_path, "combine_static_output")
        if os.path.exists(tmp_output):
            shutil.rmtree(tmp_output)

        os.mkdir(tmp_output)

        # There is no --output option for 'ar -x' on macos.
        os.chdir(tmp_output)
        returncode = 0

        try:
            for library in arg_info.library[1:]:
                ar_extract(library, args.log_level)
            ar_create(target_library, tmp_output, args.log_level)
        except Exception as e:
            returncode = -1
            print(f"An error occurred: {e}")
        finally:
            shutil.rmtree(tmp_output)
            os.chdir(origin_wd)
            sys.exit(-1 if returncode != 0 else 0)
