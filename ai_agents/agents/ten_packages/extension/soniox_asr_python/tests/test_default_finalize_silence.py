#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

# ABOUTME: Tests for optional silence packets in DEFAULT finalize mode.
# ABOUTME: Verifies silence is sent before finalize and excluded from timestamps.

import asyncio
import json

from ..websocket import (
    SonioxFinToken,
    SonioxTranscriptToken,
)
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override


class DefaultFinalizeSilenceTester(AsyncExtensionTester):
    """Tests that silence packets are sent before finalize message."""

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.finalize_id = "test-finalize-silence"
        # Track call order
        self.call_order: list[str] = []

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        # Send some audio frames first
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
            await asyncio.sleep(0.05)

        # Send finalize data event
        if not self.stopped:
            await asyncio.sleep(0.5)
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

        if data_name == "asr_finalize_end":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(
                f"tester on_data, asr_finalize_end data_dict: {data_dict}"
            )

            # Validate finalize_id
            self.stop_test_if_checking_failed(
                ten_env_tester,
                data_dict.get("finalize_id") == self.finalize_id,
                f"finalize_id mismatch: expected {self.finalize_id}, got {data_dict.get('finalize_id')}",
            )

            # Verify call order: silence audio should be sent BEFORE finalize
            self.stop_test_if_checking_failed(
                ten_env_tester,
                len(self.call_order) >= 2,
                f"Expected at least 2 calls, got {len(self.call_order)}: {self.call_order}",
            )

            # Find the indices of silence_audio and finalize
            silence_index = None
            finalize_index = None
            for i, call in enumerate(self.call_order):
                if call == "silence_audio" and silence_index is None:
                    silence_index = i
                if call == "finalize" and finalize_index is None:
                    finalize_index = i

            self.stop_test_if_checking_failed(
                ten_env_tester,
                silence_index is not None,
                f"silence_audio not found in call_order: {self.call_order}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                finalize_index is not None,
                f"finalize not found in call_order: {self.call_order}",
            )

            if silence_index is not None and finalize_index is not None:
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    silence_index < finalize_index,
                    f"silence_audio (index {silence_index}) should come BEFORE finalize (index {finalize_index}): {self.call_order}",
                )

            # Test passed
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


def test_silence_packets_sent_before_finalize(patch_soniox_ws):
    """Test that silence packets are sent BEFORE finalize message when enabled."""
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    tester = DefaultFinalizeSilenceTester()
    silence_duration_ms = 800
    sample_rate = 16000
    expected_silence_bytes = int(silence_duration_ms * sample_rate / 1000 * 2)

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)

    async def custom_send_audio(audio_data: bytes):
        # Track silence audio (zero-filled bytes of expected length)
        if len(audio_data) == expected_silence_bytes:
            # Verify it's silence (all zeros)
            if all(b == 0 for b in audio_data):
                tester.call_order.append("silence_audio")
        await asyncio.sleep(0)

    async def custom_finalize(
        trailing_silence_ms=None, before_send_callback=None
    ):
        tester.call_order.append("finalize")
        await asyncio.sleep(0.1)

        if before_send_callback:
            await before_send_callback()

        # Send final transcript
        final_token = SonioxTranscriptToken(
            text="hello world",
            start_ms=0,
            end_ms=500,
            is_final=True,
            language="en",
        )
        fin_token = SonioxFinToken("<fin>", True)

        await patch_soniox_ws.websocket_client.trigger_transcript(
            [final_token, fin_token], 500, 500
        )
        await patch_soniox_ws.websocket_client.trigger_finished(500, 500)

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
        on_send_audio=custom_send_audio,
        on_finalize=custom_finalize,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": sample_rate,
        },
        "default_finalize_send_silence": True,
        "default_finalize_silence_duration_ms": silence_duration_ms,
    }

    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_silence_packets_sent_before_finalize err: {err}"


def test_silence_bytes_have_correct_length(patch_soniox_ws):
    """Test that silence bytes have the correct length based on duration and sample rate."""
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    silence_duration_ms = 500
    sample_rate = 16000
    expected_silence_bytes = int(silence_duration_ms * sample_rate / 1000 * 2)
    captured_silence_bytes: list[int] = []

    class SilenceLengthTester(AsyncExtensionTester):
        def __init__(self):
            super().__init__()
            self.sender_task: asyncio.Task[None] | None = None
            self.stopped = False

        async def audio_sender(self, ten_env: AsyncTenEnvTester):
            # Send one audio frame
            chunk = b"\x01\x02" * 160
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)

            await asyncio.sleep(0.3)

            # Send finalize
            finalize_data = Data.create("asr_finalize")
            finalize_data.set_property_string("finalize_id", "test-123")
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

        @override
        async def on_data(
            self, ten_env_tester: AsyncTenEnvTester, data: Data
        ) -> None:
            if data.get_name() == "asr_finalize_end":
                # Test captured silence bytes length
                if captured_silence_bytes:
                    if captured_silence_bytes[0] == expected_silence_bytes:
                        ten_env_tester.stop_test()
                    else:
                        err = TenError.create(
                            error_code=TenErrorCode.ErrorCodeGeneric,
                            error_message=f"Silence bytes length mismatch: expected {expected_silence_bytes}, got {captured_silence_bytes[0]}",
                        )
                        ten_env_tester.stop_test(err)
                else:
                    err = TenError.create(
                        error_code=TenErrorCode.ErrorCodeGeneric,
                        error_message="No silence bytes captured",
                    )
                    ten_env_tester.stop_test(err)

        @override
        async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
            self.stopped = True
            if self.sender_task:
                _ = self.sender_task.cancel()
                try:
                    await self.sender_task
                except asyncio.CancelledError:
                    pass

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)

    async def custom_send_audio(audio_data: bytes):
        # Capture silence audio (all zeros)
        if all(b == 0 for b in audio_data):
            captured_silence_bytes.append(len(audio_data))
        await asyncio.sleep(0)

    async def custom_finalize(
        trailing_silence_ms=None, before_send_callback=None
    ):
        await asyncio.sleep(0.1)
        if before_send_callback:
            await before_send_callback()

        final_token = SonioxTranscriptToken(
            text="test", start_ms=0, end_ms=100, is_final=True, language="en"
        )
        fin_token = SonioxFinToken("<fin>", True)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [final_token, fin_token], 100, 100
        )
        await patch_soniox_ws.websocket_client.trigger_finished(100, 100)

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
        on_send_audio=custom_send_audio,
        on_finalize=custom_finalize,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "sample_rate": sample_rate,
        },
        "default_finalize_send_silence": True,
        "default_finalize_silence_duration_ms": silence_duration_ms,
    }

    tester = SilenceLengthTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_silence_bytes_have_correct_length err: {err}"


def test_timestamp_excludes_silence(patch_soniox_ws):
    """Test that ASR result timestamps exclude silence audio (only count user audio)."""
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    silence_duration_ms = 800
    sample_rate = 16000
    user_audio_duration_ms = (
        60  # 3 frames * 20ms each (320 bytes = 20ms at 16kHz)
    )

    class TimestampTester(AsyncExtensionTester):
        def __init__(self):
            super().__init__()
            self.sender_task: asyncio.Task[None] | None = None
            self.stopped = False
            self.asr_result_received = False
            self.asr_result_start_ms: int | None = None

        async def audio_sender(self, ten_env: AsyncTenEnvTester):
            # Send 3 audio frames (each 320 bytes = 20ms at 16kHz 16-bit)
            for i in range(3):
                if self.stopped:
                    break
                chunk = b"\x01\x02" * 160  # 320 bytes
                audio_frame = AudioFrame.create("pcm_frame")
                metadata = {"session_id": "123"}
                audio_frame.set_property_from_json(
                    "metadata", json.dumps(metadata)
                )
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(0.02)

            await asyncio.sleep(0.3)

            # Send finalize (this will add silence)
            finalize_data = Data.create("asr_finalize")
            finalize_data.set_property_string("finalize_id", "test-ts")
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

        @override
        async def on_data(
            self, ten_env_tester: AsyncTenEnvTester, data: Data
        ) -> None:
            data_name = data.get_name()

            if data_name == "asr_result":
                data_json, _ = data.get_property_to_json()
                data_dict = json.loads(data_json)
                ten_env_tester.log_info(f"asr_result: {data_dict}")

                if data_dict.get("final"):
                    self.asr_result_received = True
                    self.asr_result_start_ms = data_dict.get("start_ms")

            elif data_name == "asr_finalize_end":
                # After finalize completes, verify timestamp
                if not self.asr_result_received:
                    err = TenError.create(
                        error_code=TenErrorCode.ErrorCodeGeneric,
                        error_message="No ASR result received",
                    )
                    ten_env_tester.stop_test(err)
                    return

                # The start_ms should be based on user audio position,
                # not including silence duration
                # Since we sent 3 frames of 20ms each, user audio is 60ms
                # Silence should NOT be included in the timestamp calculation
                if self.asr_result_start_ms is not None:
                    # The adjusted timestamp should be <= user_audio_duration
                    # (it's the audio position before silence was added)
                    if self.asr_result_start_ms <= user_audio_duration_ms:
                        ten_env_tester.stop_test()
                    else:
                        err = TenError.create(
                            error_code=TenErrorCode.ErrorCodeGeneric,
                            error_message=f"Timestamp includes silence: start_ms={self.asr_result_start_ms}, expected <= {user_audio_duration_ms}",
                        )
                        ten_env_tester.stop_test(err)
                else:
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

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)

    async def custom_send_audio(audio_data: bytes):
        await asyncio.sleep(0)

    async def custom_finalize(
        trailing_silence_ms=None, before_send_callback=None
    ):
        await asyncio.sleep(0.1)
        if before_send_callback:
            await before_send_callback()

        # Return final result with start_ms at beginning of audio
        final_token = SonioxTranscriptToken(
            text="hello",
            start_ms=0,  # Start at beginning
            end_ms=50,
            is_final=True,
            language="en",
        )
        fin_token = SonioxFinToken("<fin>", True)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [final_token, fin_token], 50, 50
        )
        await patch_soniox_ws.websocket_client.trigger_finished(50, 50)

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
        on_send_audio=custom_send_audio,
        on_finalize=custom_finalize,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "sample_rate": sample_rate,
        },
        "default_finalize_send_silence": True,
        "default_finalize_silence_duration_ms": silence_duration_ms,
    }

    tester = TimestampTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_timestamp_excludes_silence err: {err}"


def test_no_silence_when_disabled(patch_soniox_ws):
    """Test that no silence packets are sent when feature is disabled (default)."""
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    silence_detected = []

    class NoSilenceTester(AsyncExtensionTester):
        def __init__(self):
            super().__init__()
            self.sender_task: asyncio.Task[None] | None = None
            self.stopped = False

        async def audio_sender(self, ten_env: AsyncTenEnvTester):
            chunk = b"\x01\x02" * 160
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)

            await asyncio.sleep(0.3)

            finalize_data = Data.create("asr_finalize")
            finalize_data.set_property_string("finalize_id", "test-no-silence")
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

        @override
        async def on_data(
            self, ten_env_tester: AsyncTenEnvTester, data: Data
        ) -> None:
            if data.get_name() == "asr_finalize_end":
                # Verify no silence was sent
                if silence_detected:
                    err = TenError.create(
                        error_code=TenErrorCode.ErrorCodeGeneric,
                        error_message=f"Unexpected silence packets detected: {len(silence_detected)}",
                    )
                    ten_env_tester.stop_test(err)
                else:
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

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)

    async def custom_send_audio(audio_data: bytes):
        # Detect silence (all zeros and significant length)
        if len(audio_data) > 1000 and all(b == 0 for b in audio_data):
            silence_detected.append(len(audio_data))
        await asyncio.sleep(0)

    async def custom_finalize(
        trailing_silence_ms=None, before_send_callback=None
    ):
        await asyncio.sleep(0.1)
        if before_send_callback:
            await before_send_callback()

        final_token = SonioxTranscriptToken(
            text="test", start_ms=0, end_ms=100, is_final=True, language="en"
        )
        fin_token = SonioxFinToken("<fin>", True)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [final_token, fin_token], 100, 100
        )
        await patch_soniox_ws.websocket_client.trigger_finished(100, 100)

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
        on_send_audio=custom_send_audio,
        on_finalize=custom_finalize,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    # Note: default_finalize_send_silence is NOT set (defaults to False)
    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "sample_rate": 16000,
        },
    }

    tester = NoSilenceTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_no_silence_when_disabled err: {err}"
