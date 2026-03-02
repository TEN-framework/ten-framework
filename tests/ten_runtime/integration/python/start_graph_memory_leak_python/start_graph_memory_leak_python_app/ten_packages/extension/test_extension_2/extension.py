#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

from ten_runtime import (
    Extension,
    TenEnv,
    LogLevel,
)


class TestExtension2(Extension):
    """A simple extension used as a subgraph node for memory leak testing.

    This extension does nothing special - it just starts and stops normally,
    allowing the test to measure whether repeated graph creation/destruction
    leaks memory.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")
        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_stop")
        ten_env.on_stop_done()
