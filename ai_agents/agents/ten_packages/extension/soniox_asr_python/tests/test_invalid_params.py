#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json

from ten_runtime import (AsyncExtensionTester, AsyncTenEnvTester, AudioFrame,
                         TenError, TenErrorCode)
from typing_extensions import override

from .mock import patch_soniox_ws  # noqa: F401


class SonioxAsrInvalidParamsTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.expected_error_received = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Try to send audio frames with invalid configuration
        while not self.stopped and not self.expected_error_received:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data) -> None:
        ten_env_tester.log_info(f"tester on_data, data: {data}")
        data_name = data.get_name()

        if data_name == "error":
            self.expected_error_received = True

            # Check the error data structure
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data, error data_dict: {data_dict}"
            )

            # Validate error structure
            if "code" not in data_dict:
                err = TenError.create(
                    error_code=TenErrorCode.ErrorCodeGeneric,
                    error_message=f"code is not in error data_dict: {data_dict}",
                )
                ten_env_tester.stop_test(err)
                return

            if "message" not in data_dict:
                err = TenError.create(
                    error_code=TenErrorCode.ErrorCodeGeneric,
                    error_message=f"message is not in error data_dict: {data_dict}",
                )
                ten_env_tester.stop_test(err)
                return

            # Expected error received, test passed
            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.stopped = True
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_invalid_params_empty_config(patch_soniox_ws):
    """Test with completely empty configuration"""

    async def fake_connect():
        # Should not be called with invalid config
        await asyncio.sleep(0)

    async def fake_send_audio(_audio_data):
        await asyncio.sleep(0)

    async def fake_finalize():
        await asyncio.sleep(0)

    async def fake_stop():
        await asyncio.sleep(0)

    # Inject into websocket client
    patch_soniox_ws.websocket_client.connect.side_effect = fake_connect
    patch_soniox_ws.websocket_client.send_audio.side_effect = fake_send_audio
    patch_soniox_ws.websocket_client.finalize.side_effect = fake_finalize
    patch_soniox_ws.websocket_client.stop.side_effect = fake_stop

    # Empty configuration should cause an error
    property_json = {}

    tester = SonioxAsrInvalidParamsTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_invalid_params_empty_config err: {err}"


def test_invalid_params_missing_api_key(patch_soniox_ws):
    """Test with missing API key"""

    async def fake_connect():
        # Should not be called with invalid config
        await asyncio.sleep(0)

    async def fake_send_audio(_audio_data):
        await asyncio.sleep(0)

    async def fake_finalize():
        await asyncio.sleep(0)

    async def fake_stop():
        await asyncio.sleep(0)

    # Inject into websocket client
    patch_soniox_ws.websocket_client.connect.side_effect = fake_connect
    patch_soniox_ws.websocket_client.send_audio.side_effect = fake_send_audio
    patch_soniox_ws.websocket_client.finalize.side_effect = fake_finalize
    patch_soniox_ws.websocket_client.stop.side_effect = fake_stop

    # Configuration without API key should cause an error
    property_json = {
        "params": {
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrInvalidParamsTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_invalid_params_missing_api_key err: {err}"
