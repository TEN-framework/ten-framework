#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Optional
import os
import aiofiles
import asyncio
import json
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    AudioFrame,
    AudioFrameDataFmt,
)


class ExtensionTesterBasic(AsyncExtensionTester):
    def __init__(self, input_pcm_file: str):
        super().__init__()
        self.input_pcm_file = input_pcm_file

        self.next_expect_cmd: str = "start_of_sentence"

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        assert os.path.isfile(
            self.input_pcm_file
        ), f"{self.input_pcm_file} is not a valid file"

        # await asyncio.sleep(2)  # before start

        ten_env.log_debug(f"start reading audio file {self.input_pcm_file}")

        chunk_size = 160 * 2  # 10ms
        async with aiofiles.open(self.input_pcm_file, "rb") as audio_file:
            while True:
                chunk = await audio_file.read(chunk_size)
                if not chunk:
                    ten_env.log_debug(
                        f"audio file {self.input_pcm_file} end at {audio_file.tell()}"
                    )
                    break

                # ten_env.log_debug(f"read {len(chunk)} bytes from {self.input_pcm_file}")

                audio_frame = AudioFrame.create("pcm_frame")
                audio_frame.set_bytes_per_sample(2)
                audio_frame.set_sample_rate(16000)
                audio_frame.set_number_of_channels(1)
                audio_frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
                audio_frame.set_samples_per_channel(len(chunk) // 2)
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                await ten_env.send_audio_frame(audio_frame)

                await asyncio.sleep(0.01)

        await asyncio.sleep(2)  # wait for done
        ten_env.stop_test()

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        if cmd_name in ["start_of_sentence", "end_of_sentence"]:
            if cmd_name == self.next_expect_cmd:
                ten_env.log_debug(f"{self.next_expect_cmd} cmd received")
                self.next_expect_cmd = (
                    "end_of_sentence"
                    if self.next_expect_cmd == "start_of_sentence"
                    else "start_of_sentence"
                )
            else:
                assert False, f"{self.next_expect_cmd} cmd not received"

        cmd_result = CmdResult.create(StatusCode.OK)
        await ten_env.return_result(cmd_result)


def test_basic():
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    test_file = os.path.join(cur_dir, "16k_en_US.pcm")

    if not os.path.isfile(test_file):
        print(f"test file {test_file} not found, skip the test")
        return

    property_json = {
        "prefix_padding_ms": 200,
        "silence_duration_ms": 500,
        "vad_threshold": 0.5,
        "hop_size_ms": 20,
        "dump": False,
        "dump_path": "",
    }

    tester = ExtensionTesterBasic(test_file)
    tester.set_test_mode_single("ten_vad_python", json.dumps(property_json))
    tester.run()
