#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import sys
import re


def read_ts_config(file_name):
    with open(file_name, "r", encoding="utf-8") as f:
        ts_config = f.read()

    # Remove comments and redundant commas before JSON parsing, because they
    # are not valid JSON contents.

    str_pattern = r'"(?:\\.|[^"])*"'

    # Remove comments.
    def keep_str_remove_comments(m):
        if m.group(0).startswith("/"):
            return ""
        else:
            return m.group(0)

    ts_config = re.sub(
        str_pattern + r"|/\*[.\n]+?\*/|//[^\n]*",
        keep_str_remove_comments,
        ts_config,
    )

    # Remove redundant commas.
    def keep_str_remove_redundant_commas(m):
        if m.group(0).startswith(","):
            return m.group(0)[1:]
        else:
            return m.group(0)

    ts_config = re.sub(
        str_pattern + r"|,(?:[\s\n]*[\]\}])",
        keep_str_remove_redundant_commas,
        ts_config,
    )

    # Parse ts_config.json.
    config = json.loads(ts_config)
    return config


if __name__ == "__main__":
    # Outputs depends on tsconfig.json, but not including this file.
    ts_config = read_ts_config(sys.argv[1])
    print(ts_config["compilerOptions"]["outDir"])
