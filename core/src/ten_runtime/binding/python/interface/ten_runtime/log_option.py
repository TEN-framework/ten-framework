#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
class LogOption:
    """Configuration class for log options, including skip parameter for
    extensibility"""

    skip: int

    def __init__(self, skip: int = 2) -> None:
        """
        Initialize log options

        Args:
            skip (int): Number of stack frames to skip, defaults to 2
        """
        self.skip = skip


# Default log option instance with skip=2
DefaultLogOption = LogOption(skip=2)
