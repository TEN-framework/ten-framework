#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json

from ten_packages.extension.soniox_asr_python.websocket import (
    SonioxFinToken, SonioxTranscriptToken)
from ten_runtime import (AsyncExtensionTester, AsyncTenEnvTester, AudioFrame,
                         Data, TenError, TenErrorCode)
from typing_extensions import override

from .mock import patch_soniox_ws  # noqa: F401


class SonioxAsrFinalizeTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.finalize_id = "test-finalize-123"

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Send some audio frames first
        for i in range(5):
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

        # Send finalize data event
        if not self.stopped:
            await asyncio.sleep(1.0)  # Wait for some processing time
            finalize_data = Data.create("asr_finalize")
            finalize_data.set_property_string("finalize_id", self.finalize_id)
            metadata = {"session_id": "123"}
            finalize_data.set_property_from_json(
                "metadata", json.dumps(metadata)
            )
            await ten_env.send_data(finalize_data)

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

        if data_name == "asr_result":
            # Validate ASR result structure
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data, asr_result data_dict: {data_dict}"
            )

            # Basic ASR result validation
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "id" in data_dict,
                f"id is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "text" in data_dict,
                f"text is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "final" in data_dict,
                f"final is not in data_dict: {data_dict}",
            )

        elif data_name == "asr_finalize_end":
            # Check the finalize end response structure
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data, asr_finalize_end data_dict: {data_dict}"
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "finalize_id" in data_dict,
                f"finalize_id is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                data_dict["finalize_id"] == self.finalize_id,
                f"finalize_id mismatch: expected {self.finalize_id}, got {data_dict.get('finalize_id')}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metadata" in data_dict,
                f"metadata is not in data_dict: {data_dict}",
            )

            # Test passed, stop the test
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


def test_finalize(patch_soniox_ws):
    async def fake_connect():
        # Simulate connection opening
        await patch_soniox_ws.websocket_client.trigger_open()

        # Wait a bit for audio to be sent
        await asyncio.sleep(0.3)

        # Send some intermediate results
        token1 = SonioxTranscriptToken(
            text="hello", start_ms=0, end_ms=500, is_final=False, language="en"
        )

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token1], 0, 0
        )
        await asyncio.sleep(0.1)

    async def fake_send_audio(_audio_data):
        await asyncio.sleep(0)

    async def fake_finalize():
        # When finalize is called, send final results
        await asyncio.sleep(0.1)

        # Send final transcript
        final_token = SonioxTranscriptToken(
            text="hello world finalized",
            start_ms=0,
            end_ms=1000,
            is_final=True,
            language="en",
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [final_token, fin_token], 1000, 1000
        )

        # Trigger finished event
        await patch_soniox_ws.websocket_client.trigger_finished(1000, 1000)

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

    tester = SonioxAsrFinalizeTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_finalize err: {err}"
