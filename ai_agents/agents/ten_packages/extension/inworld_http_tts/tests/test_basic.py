import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from pathlib import Path
import json
from unittest.mock import patch, AsyncMock
import asyncio
import threading

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush, TTS2HttpResponseEventType


# ================ test basic TTS functionality ================
class ExtensionTesterBasic(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Basic test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello world, hello inworld",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and collects their data using the lock/unlock pattern."""
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            audio_frame.unlock_buf(buf)


@patch("inworld_http_tts.extension.InworldTTSClient")
def test_basic_tts(MockInworldTTSClient):
    """Tests basic TTS functionality with mocked client."""
    print("Starting test_basic_tts with mock...")

    mock_instance = MockInworldTTSClient.return_value
    mock_instance.clean = AsyncMock()

    # Create some fake audio data to be streamed
    fake_audio_chunk_1 = b"\x11\x22\x33\x44" * 20
    fake_audio_chunk_2 = b"\xaa\xbb\xcc\xdd" * 20

    # This async generator simulates the TTS client's get() method
    async def mock_get_audio_stream(text: str, request_id: str | None = None):
        yield (fake_audio_chunk_1, TTS2HttpResponseEventType.RESPONSE)
        await asyncio.sleep(0.01)
        yield (fake_audio_chunk_2, TTS2HttpResponseEventType.RESPONSE)
        await asyncio.sleep(0.01)
        yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterBasic()

    config = {
        "params": {
            "api_key": "test_api_key",
            "voice": "en-us-f-1",
            "sampleRate": 16000,
        },
    }

    tester.set_test_mode_single("inworld_http_tts", json.dumps(config))

    print("Running basic test...")
    tester.run()
    print("Basic test completed.")

    assert tester.audio_end_received, "Expected to receive tts_audio_end"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "Expected to receive audio chunks"

    print(
        f"Basic TTS test passed: received {len(tester.received_audio_chunks)} audio chunks"
    )


# ================ test flush logic ================
class ExtensionTesterFlush(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ten_env: TenEnvTester | None = None
        self.audio_start_received = False
        self.first_audio_frame_received = False
        self.flush_start_received = False
        self.audio_end_received = False
        self.flush_end_received = False
        self.audio_received_after_flush_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("Flush test started, sending long TTS request.")
        tts_input = TTSTextInput(
            request_id="tts_request_for_flush",
            text="This is a very long text designed to generate a continuous stream of audio, providing enough time to send a flush command.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        if self.flush_end_received:
            ten_env.log_error("Received audio frame after tts_flush_end!")
            self.audio_received_after_flush_end = True

        if not self.first_audio_frame_received:
            self.first_audio_frame_received = True
            ten_env.log_info("First audio frame received, sending flush data.")
            flush_data = Data.create("tts_flush")
            flush_data.set_property_from_json(
                None,
                TTSFlush(flush_id="tts_request_for_flush").model_dump_json(),
            )
            ten_env.send_data(flush_data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "tts_audio_start":
            self.audio_start_received = True
            return

        if name == "tts_flush_start":
            self.flush_start_received = True
            return

        if name == "tts_audio_end":
            self.audio_end_received = True

        elif name == "tts_flush_end":
            self.flush_end_received = True

            def stop_test_later():
                ten_env.log_info("Waited after flush_end, stopping test now.")
                ten_env.stop_test()

            timer = threading.Timer(0.5, stop_test_later)
            timer.start()


@patch("inworld_http_tts.extension.InworldTTSClient")
def test_flush_logic(MockInworldTTSClient):
    """
    Tests that sending a flush command during TTS streaming correctly stops
    the audio and sends the appropriate events.
    """
    print("Starting test_flush_logic with mock...")

    mock_instance = MockInworldTTSClient.return_value
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()

    async def mock_get_long_audio_stream(
        text: str, request_id: str | None = None
    ):
        for _ in range(20):
            if mock_instance.cancel.called:
                print("Mock detected cancel call, sending EVENT_TTS_FLUSH.")
                yield (None, TTS2HttpResponseEventType.FLUSH)
                return
            yield (b"\x11\x22\x33" * 100, TTS2HttpResponseEventType.RESPONSE)
            await asyncio.sleep(0.1)

        yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get_long_audio_stream

    config = {
        "params": {
            "api_key": "test_api_key",
            "voice": "en-us-f-1",
        },
    }
    tester = ExtensionTesterFlush()
    tester.set_test_mode_single("inworld_http_tts", json.dumps(config))

    print("Running flush logic test...")
    tester.run()
    print("Flush logic test completed.")

    assert tester.audio_start_received, "Did not receive tts_audio_start."
    assert tester.first_audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive tts_audio_end."
    assert tester.flush_end_received, "Did not receive tts_flush_end."
    assert (
        not tester.audio_received_after_flush_end
    ), "Received audio after tts_flush_end."

    print("Flush logic test passed successfully.")
