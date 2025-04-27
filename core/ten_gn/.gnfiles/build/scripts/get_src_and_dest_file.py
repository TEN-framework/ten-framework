#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import json
from typing import Optional
from dataclasses import dataclass, asdict


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.input_string: str
        # The delimiter that separates the source and the target.
        self.src_dest_delimiter: Optional[str] = None
        # The delimiter that separates the source_base.
        self.src_base_delimiter: Optional[str] = None


@dataclass
class SrcDestInfo:
    source_base: str
    source: str
    destination: str


def get_src_and_dest_file(
    input_string: str,
    src_base_delimiter: Optional[str],
    src_dest_delimiter: Optional[str],
) -> SrcDestInfo:
    source_base: Optional[str] = None
    source: str = ""
    destination: Optional[str] = None

    remaining_string = input_string

    # Handle source_base delimiter if specified.
    if src_base_delimiter:
        parts = remaining_string.split(src_base_delimiter, 1)
        if len(parts) == 2:
            source_base, remaining_string = parts
        else:
            source_base = None

    # Handle src/dest delimiter if specified.
    if src_dest_delimiter:
        parts = remaining_string.split(src_dest_delimiter, 1)
        if len(parts) == 2:
            source_part, destination = parts
        else:
            source_part = remaining_string
            destination = None
    else:
        source_part = remaining_string
        destination = None

    # Handle source delimiter if specified.
    source = source_part

    if destination is None:
        destination = source

    return SrcDestInfo(
        source_base=source_base if source_base is not None else "",
        source=source,
        destination=destination,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Split an input string and return JSON of parts."
    )
    parser.add_argument(
        "--input-string", type=str, required=True, help="String to be split."
    )
    parser.add_argument(
        "--src-dest-delimiter",
        type=str,
        required=False,
        help="Delimiter between source and destination.",
    )
    parser.add_argument(
        "--src-base-delimiter",
        type=str,
        required=False,
        help="Delimiter for source base.",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    result = get_src_and_dest_file(
        args.input_string, args.src_base_delimiter, args.src_dest_delimiter
    )

    print(json.dumps(asdict(result), indent=2))
