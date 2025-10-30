#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#


class SendOptions:
    """Configuration options for sending messages.

    Attributes:
        wait_for_result: Whether to wait for the send result. If False,
                         the send operation will not wait for completion and
                         will not return error information, thus avoiding the
                         creation of additional asyncio tasks.
                         Defaults to False for optimal performance.
    """

    wait_for_result: bool

    def __init__(self, wait_for_result: bool = False) -> None:
        """Initialize SendOptions.

        Args:
            wait_for_result: Whether to wait for the send result.
                             Defaults to False for optimal performance.
        """
        self.wait_for_result = wait_for_result
