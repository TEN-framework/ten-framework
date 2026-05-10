#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from pathlib import Path


CACHEDIR_TAG_FILENAME = "CACHEDIR.TAG"
CACHEDIR_TAG_CONTENT = (
    "Signature: 8a477f597d28d172789f06886806bc55\n"
    "# This file is a cache directory tag created by cargo.\n"
    "# For information about cache directory tags see "
    "https://bford.info/cachedir/\n"
)


def ensure_cargo_target_dir_tag(target_dir: str | Path) -> None:
    target_dir_path = Path(target_dir)
    target_dir_path.mkdir(parents=True, exist_ok=True)

    tag_path = target_dir_path / CACHEDIR_TAG_FILENAME
    if tag_path.is_file():
        existing_content = tag_path.read_text(encoding="utf-8")
        if existing_content == CACHEDIR_TAG_CONTENT:
            return

    tag_path.write_text(CACHEDIR_TAG_CONTENT, encoding="utf-8")
