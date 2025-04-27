#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import argparse


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.input_file: str
        self.output_file: str
        self.log_level: int
        self.os_arch_pairs: list[str]


def update_supports(
    input_file: str,
    output_file: str,
    os_arch_pairs: list[list[str]],
    log_level: int,
):
    with open(input_file, "r", encoding="utf-8") as file:
        data = json.load(file)

    supports = [{"os": pair[0], "arch": pair[1]} for pair in os_arch_pairs]

    if "supports" in data:
        del data["supports"]

    data["supports"] = supports

    with open(output_file, "w", encoding="utf-8") as file:
        json.dump(data, file, indent=2)

    if log_level > 0:
        print(f"Updated {output_file} with new 'supports' field.")


def main():
    parser = argparse.ArgumentParser(
        description="Add OS:Arch pairs to the supports field of a JSON file."
    )

    parser.add_argument("--input-file", type=str, help="Input JSON file path")
    parser.add_argument("--output-file", type=str, help="Output JSON file path")
    parser.add_argument(
        "--log-level", type=int, required=False, help="specify log level"
    )
    parser.add_argument(
        "--os-arch-pairs",
        metavar="os:arch",
        type=str,
        nargs="+",
        help="OS:Arch pairs (e.g., mac:x64 linux:arm)",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    os_arch_pairs = [pair.split(":") for pair in args.os_arch_pairs]

    for pair in os_arch_pairs:
        if (
            len(pair) != 2
            or pair[0] not in ["linux", "mac", "win"]
            or pair[1] not in ["x86", "x64", "arm", "arm64"]
        ):
            print(f"Invalid OS:Arch pair: {':'.join(pair)}")
            return

    update_supports(
        args.input_file, args.output_file, os_arch_pairs, args.log_level
    )


if __name__ == "__main__":
    main()
