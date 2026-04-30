#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch, AsyncMock
import os
import asyncio
import filecmp
import shutil

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from deepgram_tts.deepgram_tts import (
    EVENT_TTS_RESPONSE,
    EVENT_TTS_END,
    EVENT_TTS_TTFB_METRIC,
)


# ================ test dump file functionality ================
class ExtensionTesterDump(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.dump_dir = "./dump/"
        self.test_dump_file_path = os.path.join(
            self.dump_dir, "test_manual_dump.pcm"
        )
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Dump test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
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
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            audio_frame.unlock_buf(buf)

    def write_test_dump_file(self):
        with open(self.test_dump_file_path, "wb") as f:
            for chunk in self.received_audio_chunks:
                f.write(chunk)

    def find_tts_dump_file(self) -> str | None:
        if not os.path.exists(self.dump_dir):
            return None
        for filename in os.listdir(self.dump_dir):
            if filename.endswith(".pcm") and filename != os.path.basename(
                self.test_dump_file_path
            ):
                return os.path.join(self.dump_dir, filename)
        return None


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_dump_functionality(MockDeepgramTTSClient):
    """Tests that the dump file from the TTS extension matches the audio received."""
    print("Starting test_dump_functionality with mock...")

    DUMP_PATH = "./dump/"

    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)
    os.makedirs(DUMP_PATH)

    mock_instance = MockDeepgramTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.reset_ttfb = lambda: None

    fake_audio_chunk_1 = b"\x11\x22\x33\x44" * 20
    fake_audio_chunk_2 = b"\xaa\xbb\xcc\xdd" * 20

    async def mock_get_audio_stream(text: str):
        yield (255, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio_chunk_1, EVENT_TTS_RESPONSE)
        await asyncio.sleep(0.01)
        yield (fake_audio_chunk_2, EVENT_TTS_RESPONSE)
        await asyncio.sleep(0.01)
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterDump()

    dump_config = {
        "dump": True,
        "dump_path": DUMP_PATH,
        "params": {
            "api_key": "test_api_key",
            "model": "aura-2-thalia-en",
            "encoding": "linear16",
            "sample_rate": 24000,
        },
    }

    tester.set_test_mode_single("deepgram_tts", json.dumps(dump_config))

    print("Running dump test...")
    tester.run()
    print("Dump test completed.")

    assert tester.audio_end_received, "Expected to receive tts_audio_end"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "Expected to receive audio chunks"

    tester.write_test_dump_file()

    tts_dump_file = tester.find_tts_dump_file()
    assert (
        tts_dump_file is not None
    ), f"Expected to find a TTS dump file in {DUMP_PATH}"
    assert os.path.exists(
        tts_dump_file
    ), f"TTS dump file should exist: {tts_dump_file}"

    print(
        f"Comparing test file {tester.test_dump_file_path} with TTS dump file {tts_dump_file}"
    )
    assert filecmp.cmp(
        tester.test_dump_file_path, tts_dump_file, shallow=False
    ), "Test dump file and TTS dump file should have the same content"

    print(
        f"Dump test passed: received {len(tester.received_audio_chunks)} audio chunks"
    )

    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)


# ================ test basic audio output ================
class ExtensionTesterBasic(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_received = False
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Basic test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_basic",
            text="Hello, this is a test of the Deepgram TTS extension.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_start":
            ten_env.log_info("Received tts_audio_start.")
            self.audio_start_received = True
        elif name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        self.audio_chunks_count += 1


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_basic_audio(MockDeepgramTTSClient):
    """Test basic TTS audio generation."""
    mock_instance = MockDeepgramTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.reset_ttfb = lambda: None

    fake_audio_chunk = b"\x00\x01\x02\x03" * 100

    async def mock_get_audio_stream(text: str):
        yield (150, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio_chunk, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterBasic()
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "model": "aura-2-thalia-en",
                    "encoding": "linear16",
                    "sample_rate": 24000,
                },
            }
        ),
    )

    tester.run()

    assert tester.audio_start_received, "tts_audio_start was not received."
    assert tester.audio_end_received, "tts_audio_end was not received."
    assert tester.audio_chunks_count > 0, "No audio chunks received."


# ================ test flush functionality ================
class ExtensionTesterFlush(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Flush test started.")

        tts_input = TTSTextInput(
            request_id="tts_request_flush",
            text="This is the first sentence.",
            text_input_end=False,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)

        flush = TTSFlush(flush_id="flush_1")
        flush_data = Data.create("tts_flush")
        flush_data.set_property_from_json(None, flush.model_dump_json())
        ten_env_tester.send_data(flush_data)

        tts_input2 = TTSTextInput(
            request_id="tts_request_flush",
            text="This is the final sentence.",
            text_input_end=True,
        )
        data2 = Data.create("tts_text_input")
        data2.set_property_from_json(None, tts_input2.model_dump_json())
        ten_env_tester.send_data(data2)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_flush(MockDeepgramTTSClient):
    """Test TTS flush functionality."""
    mock_instance = MockDeepgramTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.reset_ttfb = lambda: None

    fake_audio_chunk = b"\x00\x01\x02\x03" * 50

    async def mock_get_audio_stream(text: str):
        yield (100, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio_chunk, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterFlush()
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "model": "aura-2-thalia-en",
                    "encoding": "linear16",
                    "sample_rate": 24000,
                },
            }
        ),
    )

    tester.run()

    assert (
        tester.audio_end_received
    ), "tts_audio_end was not received after flush."
