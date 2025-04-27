#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
from typing import Optional
from build.scripts import touch

# A unique file extension is used to significantly reduce the likelihood of name
# conflicts between timestamp files and non-timestamp files.
TG_TIMESTAMP_PROXY_EXTENSION = ".tg_timestamp_proxy"


def _gen_timestamp_proxy_path(path: Optional[str]) -> Optional[str]:
    if path:
        return path + TG_TIMESTAMP_PROXY_EXTENSION
    else:
        return None


def touch_timestamp_proxy_file(path: Optional[str]) -> None:
    """
    A timestamp file is typically one of a target's outputs, and the act of
    creating or updating the timestamp file ensures that GN, during the next
    build, recognizes that the timestamp file is up-to-date and therefore
    determines that the target does not need to be re-executed.

    Args:
        path (Optional[str]): The timestamp file path.
    """
    path = _gen_timestamp_proxy_path(path)

    if path:
        try:
            touch.touch(path)
        except Exception as e:
            raise RuntimeError(
                f"Failed to touch timestamp proxy file: {str(e)}"
            ) from e


def remove_timestamp_proxy_file(path: Optional[str]) -> None:
    """
    A timestamp file is typically one of a target's outputs, and deleting the
    timestamp file effectively causes GN to determine during the next build that
    the target associated with that timestamp file needs to be executed again.

    Args:
        path (Optional[str]): The timestamp file path.
    """
    path = _gen_timestamp_proxy_path(path)

    if path and os.path.exists(path):
        try:
            os.remove(path)
        except Exception as e:
            raise OSError(
                f"Failed to remove timestamp proxy file: {str(e)}"
            ) from e
