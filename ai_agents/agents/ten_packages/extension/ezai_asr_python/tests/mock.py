#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import pytest
from unittest.mock import patch


class _FakeRecognition:
    """Fake DeepgramASRRecognition for nova model path."""

    def __init__(self, api_key, audio_timeline, ten_env, config, callback):
        self.callback = callback
        self.ten_env = ten_env

    async def start(self, timeout=10):
        # Emit connection open, then final result (nova format)
        async def _emit():
            await asyncio.sleep(0.5)
            await self.callback.on_open()
            final_msg = {
                "type": "Results",
                "channel": {"alternatives": [{"transcript": "hello world"}]},
                "is_final": True,
                "start": 0.0,
                "duration": 2.0,
            }
            await self.callback.on_result(final_msg)

        asyncio.create_task(_emit())
        return None

    async def send_audio_frame(self, audio_data):
        return None

    async def stop(self):
        return None

    async def close(self):
        return None

    def is_connected(self):
        return True


@pytest.fixture(scope="function")
def patch_ezai_ws():
    """
    Patch DeepgramASRRecognition used by ezai_asr_python extension.
    """
    patch_target = "ten_packages.extension.ezai_asr_python.extension.DeepgramASRRecognition"

    with patch(patch_target) as MockRecognition:
        print(f"✅ Patching {patch_target}")

        def _factory(api_key, audio_timeline, ten_env, config, callback):
            return _FakeRecognition(
                api_key, audio_timeline, ten_env, config, callback
            )

        MockRecognition.side_effect = _factory
        yield MockRecognition
