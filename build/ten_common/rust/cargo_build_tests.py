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
from pathlib import Path

from build.scripts import cmd_exec, fs_utils, timestamp_proxy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import target_dir_tag


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.no_run: bool
        self.project_path: str
        self.build_type: str
        self.target_path: str
        self.env: list[str]
        self.log_level: int
        self.test_output_dir: str
        self.tg_timestamp_proxy_file: str | None = None
        self.integration_test_output_name: str | None = None


def get_crate_test_info(log_level: int) -> tuple[str, str]:
    cmd = ["cargo", "metadata", "--no-deps", "--format-version", "1"]
    returncode, output = cmd_exec.run_cmd(cmd, log_level)
    if returncode:
        raise RuntimeError(f"Failed to get crate name: {output}")

    # Get the last line of the output, as there might be some note or warning
    # messages from cargo.
    output = output.splitlines()[-1]
    metadata = json.loads(output)
    package = metadata["packages"][0]
    for target in package["targets"]:
        if target["test"]:
            return package["id"], target["name"]

    raise RuntimeError("Failed to get crate name from targets.")


def parse_cargo_test_binaries(
    cargo_output: str, package_id: str
) -> tuple[dict[str, str], str]:
    test_binaries: dict[str, str] = {}
    console_lines: list[str] = []

    for line in cargo_output.splitlines():
        try:
            message = json.loads(line)
        except json.JSONDecodeError:
            console_lines.append(line)
            continue

        if not isinstance(message, dict):
            continue

        if message.get("reason") == "compiler-message":
            rendered = message.get("message", {}).get("rendered")
            if rendered:
                console_lines.append(rendered.rstrip())
            continue

        if (
            message.get("reason") != "compiler-artifact"
            or message.get("package_id") != package_id
            or not message.get("profile", {}).get("test")
            or not message.get("executable")
        ):
            continue

        target_name = message.get("target", {}).get("name")
        if target_name:
            test_binaries[target_name] = message["executable"]

    return test_binaries, "\n".join(console_lines)


def copy_test_binary(
    test_binaries: dict[str, str],
    cargo_target_name: str,
    output_dir: str,
    target_name: str,
    log_level: int = 0,
):
    source = test_binaries.get(cargo_target_name)
    if not source:
        available_targets = ", ".join(sorted(test_binaries)) or "none"
        raise RuntimeError(
            f"Cargo did not report a test binary for {cargo_target_name}. "
            f"Available test targets: {available_targets}"
        )

    if log_level > 0:
        print(
            f"Copying test binary {cargo_target_name} from {source} "
            f"to {output_dir}"
        )

    extension = ".exe" if source.lower().endswith(".exe") else ""
    fs_utils.copy_file(
        source,
        os.path.join(output_dir, target_name + extension),
        True,
    )


def run_clippy_static_checking(args: ArgumentInfo):
    cmd = [
        "cargo",
        "clippy",
        "--target-dir",
        args.target_path,
        "--target",
        args.target,
        "--tests",
    ]

    if args.build_type == "release":
        cmd.append("--release")

    cmd += [
        "--",
        "-D",
        "warnings",
    ]

    returncode, logs = cmd_exec.run_cmd(cmd, args.log_level)
    if returncode:
        raise RuntimeError(f"Failed to cargo clippy rust tests: {logs}")
    else:
        print(logs)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--no-run", type=bool, default=True)
    parser.add_argument("--project-path", type=str, required=True)
    parser.add_argument("--manifest-path", type=str, required=False)
    parser.add_argument("--build-type", type=str, required=True)
    parser.add_argument("--target-path", type=str, required=True)
    parser.add_argument("--target", type=str, required=True)
    parser.add_argument("--env", type=str, action="append", default=[])
    parser.add_argument("--log-level", type=int, required=True)
    parser.add_argument(
        "--test-output-dir",
        type=str,
        default="",
        required=False,
        help="The test executable will eventually be copied here.",
    )
    parser.add_argument(
        "--tg-timestamp-proxy-file", type=str, default="", required=False
    )
    parser.add_argument(
        "--integration-test-output-name", type=str, required=False
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    # Setup environment variables.
    for env in args.env:
        split_key_index = str(env).find("=")
        if split_key_index == -1:
            sys.exit(1)
        else:
            os.environ[(str(env)[:split_key_index])] = str(env)[
                split_key_index + len("=") :  # noqa
            ]

    origin_wd = os.getcwd()

    returncode = 0
    try:
        os.chdir(args.project_path)

        # run_clippy_static_checking(args)

        # Cargo's JSON messages provide artifact paths without relying on its
        # internal target directory layout.
        package_id, unit_test_output_name = get_crate_test_info(args.log_level)

        # cargo build --tests: only compile the test source files, without
        # running the test cases.
        # cargo test: compile the test source files and run the test cases.
        #
        # `cargo build --tests` or `cargo test --no-run` will not trigger the
        # `runner` script in .cargo/config.toml.
        if args.no_run:
            cmd = [
                "cargo",
                "build",
                "--target-dir",
                args.target_path,
                "--target",
                args.target,
                "--tests",
                "--message-format=json-render-diagnostics",
            ]
        else:
            cmd = [
                "cargo",
                "test",
                "--target-dir",
                args.target_path,
                "--target",
                args.target,
                "--message-format=json-render-diagnostics",
            ]

        if args.build_type == "release":
            cmd.append("--release")

        returncode, logs = cmd_exec.run_cmd(cmd, args.log_level)
        test_binaries, console_logs = parse_cargo_test_binaries(
            logs, package_id
        )
        if returncode:
            raise RuntimeError(
                f"Failed to build rust tests: {console_logs or logs}"
            )
        if console_logs:
            print(console_logs)

        target_dir_tag.ensure_cargo_target_dir_tag(args.target_path)

        if args.test_output_dir != "":
            # Copy the unit test binary.
            copy_test_binary(
                test_binaries,
                unit_test_output_name,
                args.test_output_dir,
                "unit_test",
                args.log_level,
            )

            if args.integration_test_output_name:
                # Copy the integration test binary.
                copy_test_binary(
                    test_binaries,
                    args.integration_test_output_name,
                    args.test_output_dir,
                    "integration_test",
                    args.log_level,
                )

        # Success to build the app, update the stamp file to represent this
        # fact.
        timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)

    except Exception as exc:
        returncode = 1
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        print(exc)

    finally:
        os.chdir(origin_wd)
        sys.exit(-1 if returncode != 0 else 0)
