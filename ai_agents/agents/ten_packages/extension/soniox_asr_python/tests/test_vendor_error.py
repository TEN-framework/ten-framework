#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json

from ten_runtime import (AsyncExtensionTester, AsyncTenEnvTester, AudioFrame,
                         Data, TenError, TenErrorCode)
from typing_extensions import override

from .mock import patch_soniox_ws  # noqa: F401


class SonioxAsrVendorErrorTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.vendor_error_received = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Send a few audio frames before triggering vendor error
        for i in range(3):
            if self.stopped:
                break
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

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        ten_env_tester.log_info(f"tester on_data, data: {data}")
        data_name = data.get_name()

        if data_name == "error" and not self.vendor_error_received:
            self.vendor_error_received = True

            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data, vendor error data_dict: {data_dict}"
            )

            # Validate vendor error structure
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "code" in data_dict,
                f"code is not in vendor error data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "message" in data_dict,
                f"message is not in vendor error data_dict: {data_dict}",
            )

            # Check if vendor-specific error information is present
            if "vendor_info" in data_dict:
                vendor_info = data_dict["vendor_info"]
                ten_env_tester.log_info(f"Vendor info received: {vendor_info}")

                # Validate vendor info structure
                if isinstance(vendor_info, dict):
                    self.stop_test_if_checking_failed(
                        ten_env_tester,
                        "error_type" in vendor_info
                        or "error_code" in vendor_info,
                        f"vendor_info should contain error_type or error_code: {vendor_info}",
                    )

            # Test passed - we received the expected vendor error
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


def test_vendor_authentication_error(patch_soniox_ws):
    async def fake_connect():
        # Simulate connection opening
        await patch_soniox_ws.websocket_client.trigger_open()

        # Wait a bit, then trigger authentication error
        await asyncio.sleep(0.2)

        # Simulate Soniox-specific authentication error
        await patch_soniox_ws.websocket_client.trigger_error(
            "auth_failed", "Invalid API key"
        )

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

    property_json = {
        "params": {
            "api_key": "invalid_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrVendorErrorTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_vendor_authentication_error err: {err}"


def test_vendor_quota_exceeded_error(patch_soniox_ws):
    async def fake_connect():
        # Simulate connection opening
        await patch_soniox_ws.websocket_client.trigger_open()

        # Wait a bit, then trigger quota exceeded error
        await asyncio.sleep(0.2)

        # Simulate Soniox-specific quota exceeded error
        await patch_soniox_ws.websocket_client.trigger_error(
            "quota_exceeded", "Monthly quota limit reached"
        )

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

    property_json = {
        "params": {
            "api_key": "valid_but_quota_exceeded_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrVendorErrorTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_vendor_quota_exceeded_error err: {err}"


def test_vendor_unsupported_format_error(patch_soniox_ws):
    async def fake_connect():
        # Simulate connection opening
        await patch_soniox_ws.websocket_client.trigger_open()

        # Wait a bit, then trigger unsupported format error
        await asyncio.sleep(0.2)

        # Simulate Soniox-specific unsupported format error
        await patch_soniox_ws.websocket_client.trigger_error(
            "unsupported_format", "Unsupported audio format"
        )

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

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 48000,  # Potentially unsupported sample rate
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrVendorErrorTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_vendor_unsupported_format_error err: {err}"


def test_vendor_service_unavailable_error(patch_soniox_ws):
    async def fake_connect():
        # Simulate service unavailable - immediate error on connect
        await asyncio.sleep(0.1)

        # Simulate Soniox service unavailable error
        await patch_soniox_ws.websocket_client.trigger_error(
            "service_unavailable",
            "Soniox transcription service is temporarily unavailable",
        )

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

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrVendorErrorTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_vendor_service_unavailable_error err: {err}"
