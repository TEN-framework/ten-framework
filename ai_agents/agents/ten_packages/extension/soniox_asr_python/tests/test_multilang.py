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


class SonioxAsrMultiLangTester(AsyncExtensionTester):

    def __init__(self, expected_language="en"):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.expected_language = expected_language
        self.results_received = []

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Send audio frames with different session metadata
        for i in range(10):
            if self.stopped:
                break
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {
                "session_id": f"multilang_session_{i}",
                "expected_language": self.expected_language,
            }
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

        if data_name == "asr_result":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(f"tester on_data, data_dict: {data_dict}")

            # Store result for analysis
            self.results_received.append(data_dict)

            # Basic structure validation
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "language" in data_dict,
                f"language is not in data_dict: {data_dict}",
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

            # Language validation
            if data_dict["language"] != self.expected_language:
                ten_env_tester.log_warn(
                    f"Language mismatch: expected {self.expected_language}, got {data_dict['language']}"
                )

            if data_dict["final"] == True and len(self.results_received) >= 2:
                # We've received enough results to validate
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


def test_english_recognition(patch_soniox_ws):
    async def fake_connect():
        await patch_soniox_ws.websocket_client.trigger_open()

        await asyncio.sleep(0.2)

        # Send English transcription results
        token1 = SonioxTranscriptToken(
            text="hello world",
            start_ms=0,
            end_ms=800,
            is_final=False,
            language="en",
        )

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token1], 0, 0
        )
        await asyncio.sleep(0.1)

        token2 = SonioxTranscriptToken(
            text="hello world this is english",
            start_ms=0,
            end_ms=1500,
            is_final=True,
            language="en",
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token2, fin_token], 1500, 1500
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
            "language_hints": ["en"],
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrMultiLangTester(expected_language="en")
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_english_recognition err: {err}"


def test_spanish_recognition(patch_soniox_ws):
    async def fake_connect():
        await patch_soniox_ws.websocket_client.trigger_open()

        await asyncio.sleep(0.2)

        # Send Spanish transcription results
        token1 = SonioxTranscriptToken(
            text="hola mundo",
            start_ms=0,
            end_ms=800,
            is_final=False,
            language="es",
        )

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token1], 0, 0
        )
        await asyncio.sleep(0.1)

        token2 = SonioxTranscriptToken(
            text="hola mundo esto es español",
            start_ms=0,
            end_ms=1500,
            is_final=True,
            language="es",
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token2, fin_token], 1500, 1500
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
            "language_hints": ["es"],
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrMultiLangTester(expected_language="es")
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_spanish_recognition err: {err}"


def test_multilang_hints(patch_soniox_ws):
    async def fake_connect():
        await patch_soniox_ws.websocket_client.trigger_open()

        await asyncio.sleep(0.2)

        # Send mixed language results
        token1 = SonioxTranscriptToken(
            text="hello", start_ms=0, end_ms=500, is_final=False, language="en"
        )

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token1], 0, 0
        )
        await asyncio.sleep(0.1)

        token2 = SonioxTranscriptToken(
            text="bonjour",
            start_ms=500,
            end_ms=1000,
            is_final=False,
            language="fr",
        )

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token2], 500, 500
        )
        await asyncio.sleep(0.1)

        token3 = SonioxTranscriptToken(
            text="hello bonjour world",
            start_ms=0,
            end_ms=1500,
            is_final=True,
            language="en",  # Dominant language
        )

        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token3, fin_token], 1500, 1500
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
            "language_hints": ["en", "fr", "es"],
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrMultiLangTester(
        expected_language="en"
    )  # Expecting dominant language
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_multilang_hints err: {err}"
