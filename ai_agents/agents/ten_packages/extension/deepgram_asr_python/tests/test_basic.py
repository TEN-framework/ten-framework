#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import json
import os
from pathlib import Path
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
)


class ExtensionTesterBasic(AsyncExtensionTester):
    def check_hello(self, ten_env: AsyncTenEnvTester, result: CmdResult):
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # TODO: move stop_test() to where the test passes
            ten_env.stop_test()

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        result, _ = await ten_env.send_cmd(new_cmd)
        self.check_hello(ten_env, result)


class ExtensionTesterDeepgram(AsyncExtensionTester):

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

        if name == "asr_result":
            json_str, _ = data.get_property_to_json(None)

            json_data = json.loads(json_str)

            language = json_data.get("language", "")
            id = json_data.get("id", "")

            assert language == "en-US"
            assert id == "user.transcription"

            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        self.sender_task.cancel()
        try:
            await self.sender_task
        except asyncio.CancelledError:
            pass


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("deepgram_asr_python")
    tester.run()

    # tester = ExtensionTesterDeepgram()
    # tester.set_test_mode_single("deepgram_asr_python", json.dumps({
    #     "api_key": os.getenv("DEEPGRAM_API_KEY", ""),
    #     "language": "en-US",
    #     "model": "nova-2",
    #     "sample_rate": 16000
    # }))
    # tester.run()
