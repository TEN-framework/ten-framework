#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import argparse
import sys
import subprocess
from build.scripts import cmd_exec, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.tman_path: str
        self.base_dir: str
        self.config_file: str
        self.log_level: int
        self.os: str
        self.cpu: str
        self.tg_timestamp_proxy_file: str


def update_manifest(
    base_dir: str, os_str: str, cpu_str: str, log_level: int
) -> None:
    manifest_path = os.path.join(base_dir, "manifest.json")

    os_arch_pair = f"{os_str}:{cpu_str}"

    script_dir = os.path.dirname(os.path.abspath(__file__))
    update_script_path = os.path.abspath(
        os.path.join(
            script_dir,
            "../../../tools/supports/update_supports_in_manifest_json.py",
        )
    )

    try:
        subprocess.run(
            [
                "python",
                update_script_path,
                "--input-file",
                manifest_path,
                "--output-file",
                manifest_path,
                "--os-arch-pairs",
                os_arch_pair,
                "--log-level",
                str(log_level),
            ],
            check=True,
        )
        if log_level > 0:
            print(
                f"Manifest updated successfully with os/cpu: {os_str}/{cpu_str}"
            )
    except subprocess.CalledProcessError as e:
        print(f"Failed to update manifest.json: {e}")
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--tman-path",
        type=str,
        required=True,
        help=(
            "Path to the tman executable. The tman is not compiled on ios or"
            " android, so it's optional."
        ),
    )
    parser.add_argument("--base-dir", type=str, required=False)
    parser.add_argument(
        "--config-file",
        default="",
        type=str,
        help="Path of tman config file",
    )
    parser.add_argument(
        "--log-level", type=int, required=False, help="specify log level"
    )
    parser.add_argument("--os", type=str, required=True)
    parser.add_argument("--cpu", type=str, required=True)
    parser.add_argument("--tg-timestamp-proxy-file", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    # The action of adding the `supports` field is currently handled within the
    # GitHub CI flow. In the future, if there is a need to perform this in the
    # GN flow, we can consider reopening this process.
    #
    # update_manifest(args.base_dir, args.os, args.cpu, args.log_level)

    # Use 'tman publish' to perform the uploading.
    origin_wd = os.getcwd()

    if args.log_level > 0:
        print(f"> Publish: {args.base_dir}")

    returncode = 0

    try:
        os.chdir(os.path.abspath(args.base_dir))

        cmd = [
            args.tman_path,
            "--config-file=" + args.config_file,
            "publish",
        ]
        returncode, output_text = cmd_exec.run_cmd(cmd)
        if returncode:
            print(output_text)
            raise RuntimeError(f"Failed to publish package: {returncode}")

        # Success to build the app, update the stamp file to represent this
        # fact.
        timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)

    except Exception as e:
        returncode = 1
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        print(e)

    finally:
        os.chdir(origin_wd)
        sys.exit(-1 if returncode != 0 else 0)
