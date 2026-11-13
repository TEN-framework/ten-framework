#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from .mock import make_extension, outputs, patch_client


def test_handshake_sends_connect_delay_metrics():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()

            (metrics,) = outputs(env, "metrics")
            assert metrics["vendor"] == "blaze"
            assert metrics["metrics"]["connect_delay"] >= 0

    asyncio.run(_run())
