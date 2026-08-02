#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import shutil
import sys
import os
from build.scripts import timestamp_proxy

# The content of the auto generated .cargo/config.toml file is as follows.
#
# ```toml
# [target.x86_64-unknown-linux-gnuasan]
# rustflags = []
#
# [build]
# target = "x86_64-unknown-linux-gnuasan"
# ```

CONFIG_TEMPLATE = """[target.{build_target}]
rustflags = []

[build]
target = "{build_target}"
{incremental_setting}
"""


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()
        self.project_root: str
        self.target: str
        self.tg_timestamp_proxy_file: str | None = None
        self.action: str
        self.disable_incremental: bool = False


def gen_cargo_config(args: ArgumentInfo):
    if not os.path.exists(args.project_root):
        raise FileNotFoundError(
            f"Project root {args.project_root} does not exist."
        )

    # Create .cargo/ folder if not exist.
    cargo_dir = os.path.join(args.project_root, ".cargo")
    if not os.path.exists(cargo_dir):
        os.mkdir(cargo_dir)

    # Check if .cargo/config.toml exists, and remove it if it does.
    cargo_config = os.path.join(cargo_dir, "config.toml")
    if os.path.exists(cargo_config):
        os.remove(cargo_config)

    incremental_setting = (
        "incremental = false" if args.disable_incremental else ""
    )

    config_content = CONFIG_TEMPLATE.format(
        build_target=args.target,
        incremental_setting=incremental_setting,
    )

    with open(cargo_config, "w", encoding="utf-8") as f:
        f.write(config_content)


def delete_cargo_config(root: str):
    cargo_dir = os.path.join(root, ".cargo")
    if os.path.exists(cargo_dir):
        shutil.rmtree(cargo_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--action", choices=("gen", "delete"), required=True)
    parser.add_argument("--project-root", type=str, required=True)
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument(
        "--tg-timestamp-proxy-file", type=str, default="", required=False
    )
    parser.add_argument(
        "--disable-incremental",
        action=argparse.BooleanOptionalAction,
        default=False,
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    returncode = 0
    if args.action == "gen":
        try:
            gen_cargo_config(args)

            # Success to gen cargo config, update the stamp file to represent
            # this fact.
            timestamp_proxy.touch_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )
        except Exception as exc:
            returncode = 1
            timestamp_proxy.remove_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )
            print(exc)

        finally:
            sys.exit(-1 if returncode != 0 else 0)
    elif args.action == "delete":
        try:
            delete_cargo_config(args.project_root)

            # Success to delete cargo config, update the stamp file to represent
            # this fact.
            timestamp_proxy.touch_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )
        except Exception as exc:
            returncode = 1
            timestamp_proxy.remove_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )
            print(exc)

        finally:
            sys.exit(-1 if returncode != 0 else 0)

