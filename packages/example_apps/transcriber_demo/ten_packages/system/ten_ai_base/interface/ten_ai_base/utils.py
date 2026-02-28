#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
def encrypt(key: str) -> str:
    step = int(len(key) / 5)
    if step > 0:
        if step > 5:
            step = 5
        prefix = key[:step]
        suffix = key[-step:]

        return f"{prefix}...{suffix}"
    else:
        return key