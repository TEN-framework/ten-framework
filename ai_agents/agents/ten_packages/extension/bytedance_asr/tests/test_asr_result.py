#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import json
import os
import threading
from time import sleep
import time
from types import SimpleNamespace
from unittest.mock import MagicMock

import pytest

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_bytedance_ws  # noqa: F401


class ExtensionTesterBytedance(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.stopped = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        ten_env.log_info("audio_sender")
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            if not chunk:
                break
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("on_start")
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        ten_env.log_info(f"on_data data: {data}")
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")
        if name == "asr_result":
            json_str, _ = data.get_property_to_json(None)
            json_data = json.loads(json_str)
            ten_env.log_info(f"on_data result: {json_data}")
            if json_data.get("text") == "hello world":
                ten_env.stop_test()
            else:
                ten_env.stop_test(
                    TenError.create(
                        TenErrorCode.ErrorCodeGeneric,
                        f"unexpected text: {json_data.get('text')}",
                    )
                )

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        self.stopped = True
        self.sender_task.cancel()
        try:
            await self.sender_task
        except asyncio.CancelledError:
            pass


def test_bytedance_basic(patch_bytedance_ws):
    tester = ExtensionTesterBytedance()
    tester.set_test_mode_single(
        "bytedance_asr",
        json.dumps(
            {
                "appid": "dummy_app_id",
                "token": "dummy_token",
                "language": "en-US",
            }
        ),
    )

    error = tester.run()

    if error is not None:
        print("Test completed with error:", error.error_message())

    assert error is None
