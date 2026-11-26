#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json

from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
)

from .mock import frames, silence_pcm, tone_pcm


class BlazeSttExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.error_data: dict | None = None

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # One utterance followed by silence so endpointing posts a request.
        for chunk in frames(tone_pcm(400) + silence_pcm(1500)):
            audio_frame = AudioFrame.create("pcm_frame")
            audio_frame.set_property_from_json(
                "metadata", json.dumps({"session_id": "123"})
            )
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.02)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() == "error":
            error_json, _ = data.get_property_to_json()
            self.error_data = json.loads(error_json)
            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            self.sender_task.cancel()


def test_error_check():
    property_json = {
        "params": {
            "base_url": "http://invalid.blaze.localhost:1",
            "api_key": "invalid_key",
        }
    }
    tester = BlazeSttExtensionTester()
    tester.set_test_mode_single("blaze_stt_python", json.dumps(property_json))
    err = tester.run()
    assert err is None
    assert tester.error_data is not None
    assert tester.error_data.get("vendor_info", {}).get("vendor") == "blaze"
