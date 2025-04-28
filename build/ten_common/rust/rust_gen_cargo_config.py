#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import json
import shutil
import sys
import os
from typing import Optional
from build.scripts import timestamp_proxy

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from scripts import package_asan_lib


# The content of the auto generated .cargo/config.toml file is as follows.
#
# - For gcc compiler:
#
# ```toml
# [target.x86_64-unknown-linux-gnu]
# rustflags = ["-C", "linker=gcc", "-Z", "external-clangrt", "-Z",
#              "sanitizer=address", "-l", "asan"]
#
# [build]
# target = "x86_64-unknown-linux-gnu"
# ```
#
# - For clang compiler:
#
# ```toml
# [target.x86_64-unknown-linux-gnu]
# rustflags = ["-C", "linker=clang", "-Z", "external-clangrt", "-Z",
#              "sanitizer=address", "-C", "link-args=-fsanitize=address"]
#
# [build]
# target = "x86_64-unknown-linux-gnu"
# ```

GCC_ASAN_FLAGS = [
    "-C",
    "linker=gcc",
    "-Z",
    "external-clangrt",
    "-Z",
    "sanitizer=address",
    "-l",
    "asan",
]


# It is better to use lld as the linker in clang. In Rust, you can specify it
# using ["-C", "link-arg=-fuse-ld=lld"]. However, lld needs to be in the PATH.
# On some CI machines, lld is not in the PATH, so we won't use lld here.
CLANG_ASAN_FLAGS = [
    "-C",
    "linker=clang",
    "-Z",
    "external-clangrt",
    "-Z",
    "sanitizer=address",
    "-C",
    "link-args=-fsanitize=address",
]

CONFIG_TEMPLATE = """[target.{build_target}]
rustflags = {asan_flags}

[build]
target = "{build_target}"
{incremental_setting}
"""


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()
        self.project_root: str
        self.build_type: str
        self.compiler: str
        self.target: str
        self.target_os: str
        self.target_arch: str
        self.tg_timestamp_proxy_file: Optional[str] = None
        self.enable_asan: bool
        self.action: str
        self.disable_incremental: bool = False


# On macOS, only the Clang compiler is available, and Clang's ASan runtime
# is provided only as a dynamic library. However, the linker flag
# '-shared-libasan' is not supported in Cargo with Clang on macOS. Cargo will
# issue a warning and ignore the flag, e.g., 'clang: warning: argument unused
# during compilation: '-shared-libasan''. This could be a bug in Cargo. To
# resolve this, use the following flag instead.
def special_link_args_on_mac(arch: str) -> str:
    asan_lib = package_asan_lib.detect_mac_asan_lib(arch)
    return f"link-arg=-Wl,{asan_lib}"


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

    flags = []
    if args.compiler == "gcc":
        flags = GCC_ASAN_FLAGS
    else:
        flags = CLANG_ASAN_FLAGS

    if args.target_os == "mac":
        flags.extend(["-C", special_link_args_on_mac(args.target_arch)])

    incremental_setting = (
        "incremental = false" if args.disable_incremental else ""
    )

    config_content = CONFIG_TEMPLATE.format(
        build_target=args.target,
        asan_flags=json.dumps(flags),
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

    parser.add_argument(
        "--action", type=str, required=True, help="gen|delete|print"
    )
    parser.add_argument("--project-root", type=str, required=False)
    parser.add_argument("--compiler", type=str, required=True)
    parser.add_argument("--target", type=str, required=False)
    parser.add_argument("--target-os", type=str, required=True)
    parser.add_argument("--target-arch", type=str, required=True)
    parser.add_argument(
        "--tg-timestamp-proxy-file", type=str, default="", required=False
    )
    parser.add_argument(
        "--enable-asan", action=argparse.BooleanOptionalAction, default=True
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

    else:
        # action = print
        #
        # Constructs and prints a space-separated string of the necessary ASan
        # flags for Clang, taking into account any macOS-specific handling.
        flags = [
            CLANG_ASAN_FLAGS[i] + CLANG_ASAN_FLAGS[i + 1]
            for i in range(0, len(CLANG_ASAN_FLAGS) - 1, 2)
        ]

        if args.target_os == "mac":
            asan_flag = special_link_args_on_mac(args.target_arch)
            flags.append(f"-C{asan_flag}")

        print(" ".join(flags))
        sys.exit(0)
