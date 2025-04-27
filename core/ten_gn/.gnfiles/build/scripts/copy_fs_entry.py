#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import argparse
from typing import Optional
from build.scripts import fs_utils, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.source: str
        self.destination: str
        self.tg_timestamp_proxy_file: Optional[str] = None
        self.files_only: bool = False


def main():
    parser = argparse.ArgumentParser(
        description="Copy source files to destination."
    )
    parser.add_argument("--source", required=True, help="Source path")
    parser.add_argument("--destination", required=True, help="Destination path")
    parser.add_argument(
        "--tg-timestamp-proxy-file",
        required=False,
        help="Destination file path",
    )
    parser.add_argument(
        "--files-only",
        action="store_true",
        default=False,
        help="Ensure source and destination are files",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    try:
        if args.files_only:
            if not os.path.isfile(args.source):
                raise FileNotFoundError(
                    f"Source '{args.source}' is not a file, "
                    "but --files-only is specified."
                )

            # Check if the destination already exists and is a directory.
            if os.path.exists(args.destination) and os.path.isdir(
                args.destination
            ):
                raise IsADirectoryError(
                    f"Destination '{args.destination}' is a directory, "
                    "but --files-only is specified."
                )

        # Check if the source is existed.
        if not os.path.exists(args.source):
            raise FileNotFoundError(f"{args.source} does not exist")

        try:
            # Ensure the destination folder, if specified, does exist.
            if os.path.dirname(args.destination) != "":
                os.makedirs(os.path.dirname(args.destination), exist_ok=True)
        except Exception as e:
            raise OSError(
                f"Failed to create destination directory: {str(e)}"
            ) from e

        try:
            fs_utils.copy(args.source, args.destination)
        except Exception as e:
            raise RuntimeError(
                f"Failed to copy {args.source} to {args.destination}: {str(e)}"
            ) from e

        # Touch the tg_timestamp_proxy_file if specified.
        timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)

    except Exception as e:
        timestamp_proxy.remove_timestamp_proxy_file(
            args.tg_timestamp_proxy_file
        )
        raise e


if __name__ == "__main__":
    main()
