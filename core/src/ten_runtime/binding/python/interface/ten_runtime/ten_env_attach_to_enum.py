#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum


class _TenEnvAttachTo(IntEnum):
    EXTENSION = 1
    EXTENSION_GROUP = 2
    APP = 3
    ADDON = 4
