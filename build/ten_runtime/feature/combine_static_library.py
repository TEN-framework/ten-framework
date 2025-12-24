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
import glob
import time
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
        self.is_mingw: bool


def ar_extract(library: str, log_level: int) -> None:
    # Check if the library file exists before trying to extract it.
    if not os.path.exists(library):
        error_msg = f"Error: Static library file does not exist: {library}"
        print(error_msg)
        raise RuntimeError(error_msg)

    # No --output option on macos.
    cmd = ["ar", "-x", library]
    returncode, output = cmd_exec.run_cmd(cmd, log_level)
    if returncode:
        print(f"Failed to extract static library '{library}': {output}")
        raise RuntimeError(f"Failed to extract static library: {library}")


def ar_create(
    target_library: str, target_path: str, log_level: int, is_mingw: bool
) -> None:
    # On MinGW, the 'ar' command doesn't support shell wildcard expansion (*.o)
    # directly in the command line, unlike on Linux/macOS where the shell expands
    # it before passing to ar. We need to explicitly expand the wildcard using
    # glob.glob() for MinGW.
    if is_mingw:
        # On MinGW, CMake may generate .obj files (Windows style) while GN generates .o files (Unix style)
        # We need to collect both types.
        obj_files = glob.glob(os.path.join(target_path, "*.o"))
        obj_files += glob.glob(os.path.join(target_path, "*.obj"))

        if not obj_files:
            error_msg = f"Error: No .o or .obj files found in {target_path}. This likely means the static library extraction failed or the library file does not exist."
            print(error_msg)
            raise RuntimeError(error_msg)

        print(f"Found {len(obj_files)} object files to combine")
        cmd = ["ar", "-rcs", target_library] + obj_files
    else:
        cmd = ["ar", "-rcs", target_library, os.path.join(target_path, "*.o")]

    returncode, output = cmd_exec.run_cmd(cmd, log_level)
    if returncode:
        print(f"Failed to create static library: {output}")
        raise RuntimeError("Failed to create static library.")


def read_path_from_env_file(env_file: str) -> str | None:
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

def safe_rmtree(path: str, max_retries: int = 3, delay: float = 0.5) -> None:
    """
    Safely remove a directory tree with retry logic for Windows.
    Windows may have file locking issues that need a short delay.
    """
    for attempt in range(max_retries):
        try:
            shutil.rmtree(path)
            return
        except PermissionError as e:
            if attempt < max_retries - 1:
                print(f"Warning: Failed to remove {path}, retrying in {delay}s... ({attempt + 1}/{max_retries})")
                time.sleep(delay)
            else:
                print(f"Error: Failed to remove {path} after {max_retries} attempts: {e}")
                raise

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--library", type=str, action="append", default=[])
    parser.add_argument("--target-path", type=str, required=True)
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument("--output", type=str, required=False)
    parser.add_argument("--env-file", type=str, required=False)
    parser.add_argument("--log-level", type=int, required=True)
    parser.add_argument("--is-mingw", action="store_true", default=False)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    is_windows = sys.platform == "win32"

    # The environment is required on Windows, we need to find lib.exe based on
    # the environment.
    if is_windows and not arg_info.is_mingw:
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

    if is_windows and not arg_info.is_mingw:
        # Use MSVC lib.exe for combining static libraries
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
        # Use ar for combining static libraries (Unix-like, including MinGW)

        # Print library info for debugging
        print(f"Combining {len(arg_info.library)} static libraries:")
        for i, lib in enumerate(arg_info.library):
            exists = "EXISTS" if os.path.exists(lib) else "NOT FOUND"
            print(f"  [{i}] {lib} [{exists}]")

        if not os.path.exists(target_library):
            if not os.path.exists(arg_info.library[0]):
                error_msg = f"Error: Base library file does not exist: {arg_info.library[0]}"
                print(error_msg)
                sys.exit(-1)
            shutil.copy(arg_info.library[0], target_library)

        origin_wd = os.getcwd()

        tmp_output = os.path.join(arg_info.target_path, "combine_static_output")
        if os.path.exists(tmp_output):
            safe_rmtree(tmp_output)

        os.mkdir(tmp_output)

        # There is no --output option for 'ar -x' on macos.
        os.chdir(tmp_output)
        returncode = 0

        try:
            for library in arg_info.library[1:]:
                ar_extract(library, args.log_level)
            ar_create(target_library, tmp_output, args.log_level, arg_info.is_mingw)

            # Print success message
            if os.path.exists(target_library):
                file_size = os.path.getsize(target_library)
                file_size_mb = file_size / (1024 * 1024)
                print(f"Successfully created combined static library: {target_library}")
                print(f"Output file size: {file_size_mb:.2f} MB ({file_size} bytes)")

        except Exception as e:
            returncode = -1
            print(f"An error occurred: {e}")
        finally:
            os.chdir(origin_wd)
            if os.path.exists(tmp_output):
                safe_rmtree(tmp_output)
            sys.exit(-1 if returncode != 0 else 0)
