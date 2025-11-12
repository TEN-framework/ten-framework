#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import argparse
import re
from build.scripts import fs_utils, timestamp_proxy


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.sources: list[str]
        self.patterns: list[str]
        self.dest_dir: str
        self.delete_dest_first: bool
        self.allow_non_exist: bool
        self.tg_timestamp_proxy_file: str


def copy_files(
    source: str, patterns: list[str], dest_dir: str, delete_dest_first: bool
) -> None:
    regex_list = []
    if len(patterns) > 0:
        for pattern in patterns:
            p = re.compile(r"{}".format(pattern))
            regex_list.append(p)

    if os.path.isfile(source):
        if len(regex_list) > 0:
            for regex in regex_list:
                if regex.match(source):
                    fs_utils.copy_file(
                        source,
                        os.path.join(dest_dir, os.path.basename(source)),
                        delete_dest_first,
                    )
                    return
        else:
            fs_utils.copy_file(
                source,
                os.path.join(dest_dir, os.path.basename(source)),
                delete_dest_first,
            )
    else:
        if len(regex_list) > 0:
            for item in os.listdir(source):
                item_path = os.path.join(source, item)

                for regex in regex_list:
                    if regex.match(item_path):
                        fs_utils.copy(
                            item_path,
                            os.path.join(dest_dir, item),
                            delete_dest_first,
                        )
                        break
        else:
            fs_utils.copy_tree(source, dest_dir, delete_dest_first)


# copy_files_with_pattern.py dir/file ['pattern1', 'pattern2'] dest
if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--sources", type=str, action="append", default=[])
    parser.add_argument("--patterns", type=str, action="append", default=[])
    parser.add_argument("--dest-dir", type=str, required=True)
    parser.add_argument("--delete-dest-first", type=bool, default=False)
    parser.add_argument("--allow-non-exist", type=bool, default=False)
    parser.add_argument(
        "--tg-timestamp-proxy-file",
        type=str,
        required=True,
        help="Path to the stamp file",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    for source in args.sources:
        if not os.path.exists(source):
            if not args.allow_non_exist:
                raise FileNotFoundError(f"The source file {source} not exists.")
        else:
            copy_files(
                source, args.patterns, args.dest_dir, args.delete_dest_first
            )

    timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)
