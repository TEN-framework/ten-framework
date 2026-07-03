#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
# test_extension_3: fast stopper in the dynamic graph.
#
# Does nothing in on_stop — returning immediately triggers on_stop_done via
# the AsyncExtension framework. With sync_stop_before_deinit enabled it cannot
# proceed to on_deinit until test_extension_2 (the slow stopper) also calls
# on_stop_done (after its 2-second sleep and on_stop_notify round-trip).
#

from ten_runtime import AsyncExtension, AsyncTenEnv


class TestExtension3(AsyncExtension):
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop: calling on_stop_done immediately")
        # Returning triggers on_stop_done automatically.
