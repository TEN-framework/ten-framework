#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import sysconfig
import platform
from build.scripts import cmd_exec


# The logic in this file is highly environment-dependent. It is designed to
# accommodate different CI environments, retrieving the necessary compilation
# parameters for building Python C extensions in each specific CI environment.
# Since different environments require different approaches to obtain these
# parameters, the methods used here are not universally applicable but are
# instead customized specifically for the CI environments in which the TEN
# framework currently operates.


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()
        self.python_version: str
        self.target_os: str
        self.config_type: str
        self.log_level: int
        self.is_mingw: bool = False
        self.ignored_cflags: list[str] = [
            "-DNDEBUG",
            "-Os",
            "-O1",
            "-O2",
            "-O3",
            "-g",
        ]


def get_embed_flags():
    config = sysconfig.get_config_vars()

    # Include directories.
    include_dirs = config.get("INCLUDEPY", "")

    # Library directories.
    lib_dirs = config.get("LIBDIR", "")

    # On Windows, LIBDIR is often None, so we need to construct it
    if not lib_dirs and sys.platform == "win32":
        import os

        python_dir = os.path.dirname(sys.executable)
        lib_dirs = os.path.join(python_dir, "libs")

    # On some builds, you might need to add 'LIBPL' or similar.
    lib_dirs_extra = config.get("LIBPL", "")
    if lib_dirs_extra:
        lib_dirs += f" {lib_dirs_extra}"

    # Libraries.
    libs = config.get("LIBS", "")
    # Include the Python library.
    python_lib = config.get("LIBRARY", "").replace("lib", "").replace(".a", "")
    if python_lib:
        libs += f" {python_lib}"
    else:
        # On Windows, if LIBRARY is not set, use pythonXY.lib format
        if sys.platform == "win32":
            version_info = sys.version_info
            python_lib = f"python{version_info.major}{version_info.minor}"
            libs += f" {python_lib}"

    # C Flags.
    cflags = config.get("CFLAGS", "")

    return {
        "include_dirs": include_dirs,
        "lib_dirs": lib_dirs,
        "libs": libs.strip(),
        "cflags": cflags.strip(),
    }


def transform_flags_for_windows(embed_flags, is_mingw=False):
    transformed = {"include_dirs": [], "lib_dirs": [], "libs": [], "cflags": []}

    # Transform include directories.
    for include_dir in embed_flags["include_dirs"].split():
        if is_mingw:
            # MinGW uses GCC-style -I flags
            transformed["include_dirs"].append(f'-I{include_dir}')
        else:
            transformed["include_dirs"].append(f'/I"{include_dir}"')

    # Transform library directories.
    for lib_dir in embed_flags["lib_dirs"].split():
        if is_mingw:
            # MinGW uses GCC-style -L flags
            transformed["lib_dirs"].append(f"-L{lib_dir}")
        elif sys.platform == "win32":
            # output without the outer quotes to avoid escaping issues
            transformed["lib_dirs"].append(f"/LIBPATH:{lib_dir}")
        else:
            transformed["lib_dirs"].append(f'/LIBPATH:"{lib_dir}"')

    # Transform libraries.
    for lib in embed_flags["libs"].split():
        # Remove '-l' if present.
        if lib.startswith("-l"):
            lib = lib[2:]

        if is_mingw:
            # MinGW uses GCC-style -l flags without .lib extension
            if lib.endswith(".lib"):
                lib = lib[:-4]  # Remove .lib extension
            transformed["libs"].append(f"{lib}")
        elif sys.platform == "win32":
            # On Windows platform, we need to remove one pair of quotes because of
            # the MSVC linker feature.
            if not lib.endswith(".lib"):
                lib = f"{lib}.lib"
            else:
                lib = lib
            transformed["libs"].append(lib)
        else:
            if not lib.endswith(".lib"):
                lib = f'"{lib}.lib"'
            else:
                lib = f'"{lib}"'
            transformed["libs"].append(lib)

    # Transform CFLAGS.
    transformed["cflags"] = embed_flags["cflags"].split()

    return transformed


def python_config_for_win(args: ArgumentInfo) -> None:
    # Retrieve embed flags using sysconfig.
    embed_flags = get_embed_flags()
    transformed_flags = transform_flags_for_windows(embed_flags, args.is_mingw)

    if args.config_type == "cflags":
        # Print include directories and any additional CFLAGS.
        for flag in transformed_flags["include_dirs"]:
            print(flag)
        for flag in transformed_flags["cflags"]:
            if flag in args.ignored_cflags:
                continue
            print(flag)
    elif args.config_type == "ldflags":
        # Print library directories.
        for lib_dir in transformed_flags["lib_dirs"]:
            print(lib_dir)
    elif args.config_type == "libs":
        # Print libraries.
        for lib in transformed_flags["libs"]:
            print(lib)
    else:
        raise ValueError(f"Unknown config_type: {args.config_type}")


def python_config_for_mac(args: ArgumentInfo) -> None:
    # is_x86_64 = platform.machine().lower() == "x86_64"

    if args.config_type == "cflags":
        # Run python-config --cflags (without --embed for extension modules)
        cmd = [
            f"python{args.python_version}-config",
            "--cflags",
        ]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for cflags.")

        outputs = output_text.split()
        for output in outputs:
            if output in args.ignored_cflags:
                continue
            print(output)

    elif args.config_type == "ldflags":
        # Run python-config --ldflags (without --embed for extension modules)
        cmd_ldflags = [
            f"python{args.python_version}-config",
            "--ldflags",
        ]

        returncode, ldflags_output = cmd_exec.run_cmd(
            cmd_ldflags, args.log_level
        )
        if returncode:
            raise RuntimeError("Failed to run python-config for ldflags.")

        ldflags_outputs = ldflags_output.split()

        # Extract framework pairs from libs. Because the `-framework <library>`
        # portion needs to be included in the `ldflags` variable in GN.
        cmd_libs = [
            f"python{args.python_version}-config",
            "--libs",
        ]
        returncode, libs_output = cmd_exec.run_cmd(cmd_libs, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for libs.")

        libs_outputs = libs_output.split()

        # Convert the `-framework <library>` portion specified in `libs` into
        # `ldflags`.
        frameworks_pairs = []
        skip_next = False
        prev_token = None
        for token in libs_outputs:
            if skip_next:
                # 'prev_token' was '-framework', 'token' is the framework name.
                frameworks_pairs.append((prev_token, token))
                skip_next = False
                continue
            if token == "-framework":
                prev_token = token
                skip_next = True

        # Add the framework pairs to ldflags.
        for pair in frameworks_pairs:
            ldflags_outputs.append(pair[0])
            ldflags_outputs.append(pair[1])

        # Note: We no longer add -L<LIBDIR> for libpython since we're building
        # extension modules (not embedding Python), which don't need to link
        # against libpython.
        #
        # if is_x86_64:
        #     libdir = sysconfig.get_config_var("LIBDIR")
        #     if libdir:
        #         ldflags_outputs.insert(0, f"-L{libdir}")

        for out in ldflags_outputs:
            print(out)

    elif args.config_type == "libs":
        # Run python-config --libs (without --embed for extension modules)
        cmd = [
            f"python{args.python_version}-config",
            "--libs",
        ]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for libs.")

        outputs = output_text.split()

        # Extract framework pairs from libs. Because the `-framework <library>`
        # portion needs to be included in the `ldflags` variable in GN.
        filtered_libs = []
        skip_next = False
        for token in outputs:
            if skip_next:
                skip_next = False
                continue
            if token == "-framework":
                skip_next = True
            else:
                filtered_libs.append(token.replace("-l", ""))  # remove -l

        for lib in filtered_libs:
            print(lib)

    else:
        raise ValueError(f"Unknown config_type: {args.config_type}")


def python_config_for_linux(args: ArgumentInfo) -> None:
    if args.config_type == "cflags":
        # Run python-config --cflags (without --embed for extension modules)
        cmd = [
            f"python{args.python_version}-config",
            "--cflags",
        ]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for cflags.")

        outputs = output_text.split()
        for output in outputs:
            if output in args.ignored_cflags:
                continue
            print(output)

    elif args.config_type == "ldflags":
        # Run python-config --ldflags (without --embed for extension modules)
        cmd_ld = [
            f"python{args.python_version}-config",
            "--ldflags",
        ]

        returncode, ld_output = cmd_exec.run_cmd(cmd_ld, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for ldflags.")

        ld_outputs = ld_output.split()

        for out in ld_outputs:
            print(out)

    elif args.config_type == "libs":
        # Run python-config --libs (without --embed for extension modules)
        cmd = [
            f"python{args.python_version}-config",
            "--libs",
        ]

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            raise RuntimeError("Failed to run python-config for libs.")

        outputs = output_text.split()

        # Remove '-l' prefixes and print
        filtered_libs = [lib.replace("-l", "") for lib in outputs]
        for lib in filtered_libs:
            print(lib)

    else:
        raise ValueError(f"Unknown config_type: {args.config_type}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--python-version", type=str, required=True)
    parser.add_argument("--target-os", type=str, required=True)
    parser.add_argument("--config-type", type=str, required=True)
    parser.add_argument("--log-level", type=int, required=True)
    parser.add_argument("--is-mingw", action="store_true", default=False)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    returncode = 0

    try:
        if args.target_os.lower() == "win":
            python_config_for_win(args)
        elif args.target_os.lower() == "mac":
            python_config_for_mac(args)
        elif args.target_os.lower() == "linux":
            python_config_for_linux(args)

    except Exception as exc:
        print(exc)
        returncode = 1

    finally:
        sys.exit(-1 if returncode != 0 else 0)
