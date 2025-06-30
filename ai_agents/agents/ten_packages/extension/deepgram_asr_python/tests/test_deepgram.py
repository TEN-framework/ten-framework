#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import json
import os
from types import SimpleNamespace

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
)


class ExtensionTesterDeepgram(AsyncExtensionTester):
    def __init__(self):
        super().__init__()

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # audio file path: ../test_data/test.pcm
        audio_file_path = os.path.join(
            os.path.dirname(__file__), "test_data/16k_en_US.pcm"
        )

        print(f"audio_file_path: {audio_file_path}")

        with open(audio_file_path, "rb") as audio_file:
            chunk_size = 320
            while True:
                chunk = audio_file.read(chunk_size)
                if not chunk:
                    break
                audio_frame = AudioFrame.create("pcm_frame")
                audio_frame.set_property_int("stream_id", 123)
                audio_frame.set_property_string("remote_user_id", "123")
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(0.01)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        # Create a task to read pcm file and send to extension
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()

        ten_env.log_info(f"on_data name: {name}")
        if name == "asr_result":
            json_str, _ = data.get_property_to_json(None)

            # json_data = json.loads(json_str)

            # language = json_data.get("language", "")

            # assert language == "en-US"

            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        self.sender_task.cancel()
        try:
            await self.sender_task
        except asyncio.CancelledError:
            pass


def test_deepgram(patch_deepgram_ws):
    async def fake_start(*args, **kwargs):
        await asyncio.sleep(1)
        handler = patch_deepgram_ws._handlers.get("Results")
        if handler:
            await handler(None, SimpleNamespace(
                channel=SimpleNamespace(
                    alternatives=[SimpleNamespace(transcript="hello world")]
                ),
                start=0.0, duration=0.5, is_final=True
            ))
        return True

    patch_deepgram_ws.start.side_effect = fake_start


    tester = ExtensionTesterDeepgram()
    tester.set_test_mode_single("deepgram_asr_python", json.dumps({
        "api_key": "111",
        "language": "en-US",
        "model": "nova-2",
        "sample_rate": 16000
    }))
    tester.run()