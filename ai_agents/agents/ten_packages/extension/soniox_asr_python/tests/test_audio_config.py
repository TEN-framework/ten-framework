#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
import os
import shutil
import tempfile

from ten_packages.extension.soniox_asr_python.const import DUMP_FILE_NAME
from ten_packages.extension.soniox_asr_python.websocket import (
    SonioxFinToken, SonioxTranscriptToken)
from ten_runtime import (AsyncExtensionTester, AsyncTenEnvTester, AudioFrame,
                         Data, TenError, TenErrorCode)
from typing_extensions import override

from .mock import patch_soniox_ws  # noqa: F401


class SonioxAsrAudioConfigTester(AsyncExtensionTester):

    def __init__(self, sample_rate=16000):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.sample_rate = sample_rate
        self.frames_sent = 0

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Calculate chunk size based on sample rate
        # For 16kHz: 160 samples * 2 bytes = 320 bytes per 10ms
        # For 48kHz: 480 samples * 2 bytes = 960 bytes per 10ms
        samples_per_10ms = self.sample_rate // 100
        chunk_size = samples_per_10ms * 2  # 16-bit samples

        while not self.stopped and self.frames_sent < 15:
            chunk = b"\x01\x02" * samples_per_10ms
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {
                "session_id": f"audio_config_test_{self.sample_rate}",
                "sample_rate": self.sample_rate,
                "frame_size": len(chunk),
            }
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            self.frames_sent += 1
            await asyncio.sleep(0.01)  # 10ms intervals

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
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data with {self.sample_rate}Hz, data_dict: {data_dict}"
            )

            # Validate structure
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

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "start_ms" in data_dict,
                f"start_ms is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "duration_ms" in data_dict,
                f"duration_ms is not in data_dict: {data_dict}",
            )

            # Validate timing makes sense for the sample rate
            if "start_ms" in data_dict and "duration_ms" in data_dict:
                start_ms = data_dict["start_ms"]
                duration_ms = data_dict["duration_ms"]

                # Basic sanity checks for timing
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    start_ms >= 0,
                    f"start_ms should be non-negative: {start_ms}",
                )

                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    duration_ms >= 0,
                    f"duration_ms should be non-negative: {duration_ms}",
                )

            if data_dict["final"] == True:
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


def test_16khz_audio(patch_soniox_ws):
    async def fake_connect():
        await patch_soniox_ws.websocket_client.trigger_open()

        await asyncio.sleep(0.15)  # Wait for audio frames

        # Send transcript for 16kHz audio
        token = SonioxTranscriptToken(
            text="sixteen kilohertz audio test",
            start_ms=0,
            end_ms=1200,
            is_final=True,
            language="en",
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token, fin_token], 1200, 1200
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

    tester = SonioxAsrAudioConfigTester(sample_rate=16000)
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_16khz_audio err: {err}"


def test_48khz_audio(patch_soniox_ws):
    async def fake_connect():
        await patch_soniox_ws.websocket_client.trigger_open()

        await asyncio.sleep(0.15)  # Wait for audio frames

        # Send transcript for 48kHz audio
        token = SonioxTranscriptToken(
            text="forty eight kilohertz high quality audio test",
            start_ms=0,
            end_ms=1500,
            is_final=True,
            language="en",
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token, fin_token], 1500, 1500
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
            "sample_rate": 48000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrAudioConfigTester(sample_rate=48000)
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_48khz_audio err: {err}"


def test_audio_dump_functionality(patch_soniox_ws):
    # Create a temporary directory for dump files
    temp_dir = tempfile.mkdtemp()
    expected_dump_file = os.path.join(temp_dir, DUMP_FILE_NAME)

    try:

        async def fake_connect():
            await patch_soniox_ws.websocket_client.trigger_open()

            await asyncio.sleep(0.15)  # Wait for audio frames

            # Send transcript
            token = SonioxTranscriptToken(
                text="audio dump test with file output",
                start_ms=0,
                end_ms=1000,
                is_final=True,
                language="en",
            )

            fin_token = SonioxFinToken("<fin>", True)

            await patch_soniox_ws.websocket_client.trigger_transcript(
                [token, fin_token], 1000, 1000
            )

        async def fake_send_audio(_audio_data):
            await asyncio.sleep(0)

        async def fake_finalize():
            await asyncio.sleep(0)

        async def fake_stop():
            await asyncio.sleep(0)

        # Inject into websocket client
        patch_soniox_ws.websocket_client.connect.side_effect = fake_connect
        patch_soniox_ws.websocket_client.send_audio.side_effect = (
            fake_send_audio
        )
        patch_soniox_ws.websocket_client.finalize.side_effect = fake_finalize
        patch_soniox_ws.websocket_client.stop.side_effect = fake_stop

        property_json = {
            "params": {
                "api_key": "fake_api_key",
                "url": "wss://fake.soniox.com/transcribe-websocket",
                "sample_rate": 16000,
                "dump": True,
                "dump_path": temp_dir,
            }
        }

        tester = SonioxAsrAudioConfigTester(sample_rate=16000)
        tester.set_test_mode_single(
            "soniox_asr_python", json.dumps(property_json)
        )
        err = tester.run()
        assert err is None, f"test_audio_dump_functionality err: {err}"

        # Verify that the dump file was created
        assert os.path.exists(
            expected_dump_file
        ), f"Dump file should exist at {expected_dump_file}"

        # Verify the dump file has content (should contain audio data)
        assert (
            os.path.getsize(expected_dump_file) > 0
        ), f"Dump file should contain audio data"

    finally:
        # Clean up temporary directory
        shutil.rmtree(temp_dir, ignore_errors=True)
