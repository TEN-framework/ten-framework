#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import json
import os
import sys
import platform
from datetime import datetime
import time
from build.scripts import fs_utils, cmd_exec, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.pkg_src_root_dir: str
        self.pkg_run_root_dir: str
        self.tg_timestamp_proxy_file: str
        self.pkg_name: str
        self.pkg_language: str
        self.os: str
        self.cpu: str
        self.build: str
        self.is_clang: bool
        self.enable_sanitizer: bool
        self.vs_version: str
        self.log_level: int


def construct_extra_args_for_cpp_ag(args: ArgumentInfo) -> list[str]:
    cmd = ["--"]

    if args.is_clang is True:
        cmd += ["is_clang=true"]
    else:
        cmd += ["is_clang=false"]

    if args.enable_sanitizer is True:
        cmd += ["enable_sanitizer=true"]
    else:
        cmd += ["enable_sanitizer=false"]

    if args.vs_version:
        cmd += [f"vs_version={args.vs_version}"]

    return cmd


def build_cpp_app(args: ArgumentInfo) -> int:
    # tgn gen ...
    cmd = ["tgn", "gen", args.os, args.cpu, args.build]
    cmd += construct_extra_args_for_cpp_ag(args)

    returncode, output = cmd_exec.run_cmd(cmd, args.log_level)

    if returncode:
        print(f"Failed to build c++ app: {output}")
        return 1

    # tgn build ...
    cmd = ["tgn", "build", args.os, args.cpu, args.build]

    t1 = datetime.now()
    returncode, output = cmd_exec.run_cmd(cmd, args.log_level)
    t2 = datetime.now()

    duration = (t2 - t1).seconds

    if args.log_level > 0:
        print(f"Build c++ app({args.pkg_name}) costs {duration} seconds.")

    if returncode:
        print(f"Failed to build c++ app: {output}")
        return 1

    return returncode


def build_cpp_extension(args: ArgumentInfo) -> int:
    # tgn gen ...
    cmd = ["tgn", "gen", args.os, args.cpu, args.build]
    cmd += construct_extra_args_for_cpp_ag(args)

    returncode, output = cmd_exec.run_cmd(cmd, args.log_level)

    if returncode:
        print(f"Failed to build c++ extension: {output}")
        return 1

    # tgn build ...
    cmd = ["tgn", "build", args.os, args.cpu, args.build]

    returncode, output = cmd_exec.run_cmd(cmd, args.log_level)

    if returncode:
        print(f"Failed to build c++ extension: {output}")
        return 1

    return returncode


def is_mac_arm64() -> bool:
    return (
        platform.system().lower() == "darwin"
        and platform.machine().lower() == "arm64"
    )


def build_go_app(args: ArgumentInfo) -> int:
    # Determine the path to the main.go script. Some cases require a customized
    # Go build script, but in most situations, the build script provided by the
    # TEN runtime Go binding can be used directly.
    main_go_path = "tools/build/main.go"
    if not os.path.exists(main_go_path):
        main_go_path = "ten_packages/system/ten_runtime_go/tools/build/main.go"

    cmd = ["go", "run", main_go_path]
    if args.log_level > 0:
        cmd += ["--verbose"]

    # `-asan` is not supported by go compiler on darwin/arm64.
    if args.enable_sanitizer and not is_mac_arm64():
        cmd += ['--build_flags="-asan"']

    envs = os.environ.copy()
    if args.is_clang:
        envs["CC"] = "clang"
        envs["CXX"] = "clang++"
    else:
        envs["CC"] = "gcc"
        envs["CXX"] = "g++"

    t1 = datetime.now()
    returncode, output = cmd_exec.run_cmd_realtime(
        cmd, None, envs, args.log_level
    )
    t2 = datetime.now()

    duration = (t2 - t1).seconds

    if args.log_level > 0:
        print(f"Build go app({args.pkg_name}) costs {duration} seconds.")

    if returncode:
        print(f"Failed to build go app: {output}")
        return 1

    return returncode


def npm_install() -> int:
    # 'npm install' might be failed because of the bad network connection, and
    # it might be stuck in a situation where it cannot be recovered. Therefore,
    # the best way is to delete the whole node_modules/, and try again.
    returncode = 0

    for _ in range(1, 10):
        returncode, output = cmd_exec.run_cmd(
            [
                "npm",
                "install",
            ]
        )

        if returncode == 0:
            break
        else:
            # Delete node_modules/ and try again.
            print(
                (
                    f"Failed to 'npm install', output: {output}, "
                    "deleting node_modules/ and try again."
                )
            )
            fs_utils.remove_tree("node_modules")
            time.sleep(5)
    if returncode != 0:
        print("Failed to 'npm install' after 10 times retries")

    return returncode


def build_nodejs_extensions(app_root_path: str) -> int:
    origin_wd = os.getcwd()

    extension_dir = os.path.join(app_root_path, "ten_packages/extension")

    if os.path.exists(extension_dir):
        for extension in os.listdir(extension_dir):
            extension_path = os.path.join(extension_dir, extension)
            if os.path.isdir(extension_path):
                # If the extension is a typescript extension, build it.
                if not os.path.exists(
                    os.path.join(extension_path, "tsconfig.json")
                ):
                    continue

                # Change to extension directory.
                os.chdir(extension_path)

                status = npm_install()
                if status != 0:
                    print(f"Failed to npm install in {extension_path}")
                    return 1

                now = datetime.now()
                cmd = ["npm", "run", "build"]
                returncode, output = cmd_exec.run_cmd(cmd)
                if returncode:
                    print(
                        (
                            f"Failed to build extension {extension_path}: "
                            f"{output}"
                        )
                    )
                    return 1

                duration = (datetime.now() - now).seconds
                print(
                    (
                        f"Profiling ====> build ts extension({extension}) "
                        f"costs {duration} seconds."
                    )
                )

    os.chdir(origin_wd)

    return 0


def build_nodejs_app(args: ArgumentInfo) -> int:
    t1 = datetime.now()

    status_code = npm_install()
    if status_code != 0:
        return status_code

    cmd = ["npm", "run", "build"]
    status_code, output = cmd_exec.run_cmd(cmd)
    if status_code != 0:
        print(f"Failed to npm run build, output: {output}")
        return status_code

    t2 = datetime.now()
    duration = (t2 - t1).seconds
    print(
        (
            f"Profiling ====> build node app({args.pkg_name}) "
            f"costs {duration} seconds."
        )
    )

    status_code = build_nodejs_extensions(os.getcwd())
    if status_code != 0:
        print(f"Failed to build ts extensions of app {args.pkg_name}")

    return status_code


def get_pkg_type(pkg_root: str) -> str:
    manifest_path = os.path.join(pkg_root, "manifest.json")
    with open(
        manifest_path,
        "r",
        encoding="utf-8",
    ) as f:
        manifest_json = json.load(f)
    return manifest_json["type"]


def build_app(args: ArgumentInfo) -> int:
    returncode = 0

    if args.pkg_language == "cpp":
        returncode = build_cpp_app(args)
    elif args.pkg_language == "go":
        returncode = build_go_app(args)
    elif args.pkg_language == "python":
        returncode = 0
    elif args.pkg_language == "nodejs":
        returncode = build_nodejs_app(args)
    else:
        print(f"Unknown app language: {args.pkg_language}")
        returncode = 1

    return returncode


def build_extension(args: ArgumentInfo) -> int:
    returncode = 0

    if args.pkg_language == "cpp":
        returncode = build_cpp_extension(args)
    else:
        returncode = 0

    return returncode


def build(args: ArgumentInfo) -> int:
    if args.log_level > 0:
        msg = (
            f"> Start to build package({args.pkg_name})"
            f" in {args.pkg_src_root_dir}"
        )
        print(msg)

    origin_wd = os.getcwd()
    returncode = 0

    try:
        os.chdir(args.pkg_src_root_dir)
        pkg_type = get_pkg_type(args.pkg_src_root_dir)
        if pkg_type == "app":
            returncode = build_app(args)
        elif pkg_type == "extension":
            returncode = build_extension(args)
        else:
            returncode = 0

        if returncode > 0:
            print(f"Failed to build {pkg_type}({args.pkg_name})")
            return 1

        # Success to build the app, update the stamp file to represent this
        # fact.
        if args.tg_timestamp_proxy_file:
            timestamp_proxy.touch_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )

        if args.log_level > 0:
            print(f"Build {pkg_type}({args.pkg_name}) success")

    except Exception as e:
        returncode = 1
        if args.tg_timestamp_proxy_file:
            timestamp_proxy.remove_timestamp_proxy_file(
                args.tg_timestamp_proxy_file
            )
        print(f"Build package({args.pkg_name}) failed: {repr(e)}")

    finally:
        os.chdir(origin_wd)

    return returncode


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--pkg-src-root-dir", type=str, required=True)
    parser.add_argument("--pkg-run-root-dir", type=str, required=False)
    parser.add_argument("--tg-timestamp-proxy-file", type=str, required=False)
    parser.add_argument("--pkg-name", type=str, required=True)
    parser.add_argument("--pkg-language", type=str, required=True)
    parser.add_argument("--os", type=str, required=True)
    parser.add_argument("--cpu", type=str, required=True)
    parser.add_argument("--build", type=str, required=True)
    parser.add_argument("--is-clang", action=argparse.BooleanOptionalAction)
    parser.add_argument(
        "--enable-sanitizer", action=argparse.BooleanOptionalAction
    )
    parser.add_argument("--vs-version", type=str, required=False)
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    return_code = build(arg_info)
    sys.exit(return_code)
