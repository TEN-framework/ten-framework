#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import sys
import os
from . import cmd_exec


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.tman_path: str
        self.pkg_type: str
        self.pkg_src_root_dir: str
        self.src_pkg: str
        self.config_file: str
        self.log_level: int
        self.local_registry_path: str


def get_pkgs_from_local_registry(
    local_registry_path: str, pkg_type: str, pkg_name: str
) -> list[str]:
    # Find directories under <local_registry_path>/<pkg_type>/<pkg_name> and
    # return their names.
    pkg_dir = os.path.join(local_registry_path, pkg_type, pkg_name)
    if not os.path.exists(pkg_dir):
        return []

    return os.listdir(pkg_dir)


def main(args: ArgumentInfo) -> int:
    origin_wd = os.getcwd()
    returncode = 0

    generated_app_name = os.path.basename(args.pkg_src_root_dir)

    try:
        # Install app.
        base_dir = os.path.dirname(args.pkg_src_root_dir)
        os.makedirs(base_dir, exist_ok=True)
        os.chdir(base_dir)

        cmd = [
            args.tman_path,
        ]

        if args.log_level > 0:
            cmd += ["--verbose"]

        if args.config_file is not None:
            list.append(cmd, "--config-file=" + args.config_file)

        if args.local_registry_path is not None:
            versions = get_pkgs_from_local_registry(
                args.local_registry_path, args.pkg_type, args.src_pkg
            )

            if len(versions) == 1:
                # If there is only one version, install using that specific
                # version number to avoid installation failures due to it being
                # a pre-release version.
                args.src_pkg += f"@{versions[0]}"

        cmd += [
            "create",
            args.pkg_type,
            generated_app_name,
            "--template",
            args.src_pkg,
        ]

        if args.log_level > 0:
            print(f"> {cmd}")

        returncode, output_text = cmd_exec.run_cmd(cmd, args.log_level)
        if returncode:
            print(output_text)
            raise RuntimeError("Failed to install app.")

    except Exception as exc:
        returncode = 1
        print(exc)

    finally:
        os.chdir(origin_wd)

    return returncode


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--tman-path",
        type=str,
        required=True,
        help="Path to tman command itself",
    )
    parser.add_argument(
        "--pkg-type",
        type=str,
        required=True,
        help="The type of package",
        choices=["app", "extension"],
    )
    parser.add_argument(
        "--pkg-src-root-dir",
        type=str,
        required=True,
        help="Path to app source root directory",
    )
    parser.add_argument(
        "--src-pkg",
        type=str,
        required=True,
        help="The name of TEN package",
    )
    parser.add_argument(
        "--config-file",
        type=str,
        required=False,
        help="Path of tman config file",
    )
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )
    parser.add_argument(
        "--local-registry-path",
        type=str,
        required=False,
        help="Path to local registry",
    )

    arg_info = ArgumentInfo()
    parsed_args = parser.parse_args(namespace=arg_info)

    sys.exit(main(parsed_args))
