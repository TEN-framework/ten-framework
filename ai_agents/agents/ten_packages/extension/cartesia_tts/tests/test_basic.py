import sys
from pathlib import Path


# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from pathlib import Path
import json
from unittest.mock import patch, AsyncMock, MagicMock
import os
import asyncio
import filecmp
import shutil
import threading
import pytest

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from cartesia_tts.cartesia_tts import CartesiaTTSClient
from cartesia_tts.config import CartesiaTTSConfig
from cartesia_tts.extension import CartesiaTTSExtension


def _make_mock_client(audio_chunks_by_request=None):
    """
    Create a mock CartesiaTTSClient for the full-duplex architecture.

    audio_chunks_by_request: dict mapping request_id -> list of (bytes|None, timestamp_ms).
        When text_to_speech is called, audio chunks are scheduled to be put into pcm_queue.
        A None bytes value signals end-of-audio.
    """
    mock = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock.start = AsyncMock()
    mock.stop = AsyncMock()
    mock.cancel = AsyncMock()
    mock.set_current_request_id = AsyncMock()
    mock.send_audio_end_signal = AsyncMock()

    # Track text_to_speech calls
    mock._tts_call_count = 0
    mock._audio_chunks_by_request = audio_chunks_by_request or {}

    async def mock_text_to_speech(t):
        mock._tts_call_count += 1
        request_id = t.request_id
        chunks = mock._audio_chunks_by_request.get(request_id, [])
        if chunks:
            # Schedule putting chunks into pcm_queue
            async def _put_chunks():
                await asyncio.sleep(0.01)
                for chunk_data, ts in chunks:
                    await pcm_queue.put((chunk_data, request_id, ts))

            asyncio.create_task(_put_chunks())

    mock.text_to_speech = AsyncMock(side_effect=mock_text_to_speech)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock.get_words = AsyncMock(side_effect=mock_get_words)

    mock._pcm_queue = pcm_queue
    mock._words_queue = words_queue

    return mock


async def _handle_timestamps_and_get_words(
    raw_words: list[str], context_id: str = "ctx-1"
):
    ten_env = MagicMock()
    client = CartesiaTTSClient(
        config=CartesiaTTSConfig(api_key="test"),
        ten_env=ten_env,
    )
    client._base_start_ms = 1000

    starts = [0.1 + i * 0.1 for i in range(len(raw_words))]
    ends = [start + 0.1 for start in starts]

    await client._handle_timestamps(
        {
            "words": raw_words,
            "start": starts,
            "end": ends,
        },
        context_id,
    )
    return client, await client.get_words()


@pytest.mark.parametrize(
    ("raw_words", "expected_text", "expected_word_tokens"),
    [
        (["is", "your"], "is your", ["is", " ", "your"]),
        ([".", "If"], ". If", [".", " ", "If"]),
        (["(", "hello"], "(hello", ["(", "hello"]),
        (["hello", ","], "hello,", ["hello", ","]),
        (["你", "好"], "你好", ["你", "好"]),
        (["中", "English"], "中English", ["中", "English"]),
        (
            ["Hello", "world", "!"],
            "Hello world!",
            ["Hello", " ", "world", "!"],
        ),
    ],
)
def test_handle_timestamps_spacing_cases(
    raw_words, expected_text, expected_word_tokens
):
    async def _run():
        _, (words, context_id, text, text_input_end) = (
            await _handle_timestamps_and_get_words(raw_words)
        )

        assert context_id == "ctx-1"
        assert text == expected_text
        assert text_input_end is False
        assert [word.word for word in words] == expected_word_tokens

    asyncio.run(_run())


def test_handle_timestamps_keeps_group_prefix_space_and_inner_spaces():
    async def _run():
        ten_env = MagicMock()
        client = CartesiaTTSClient(
            config=CartesiaTTSConfig(api_key="test"),
            ten_env=ten_env,
        )
        client._base_start_ms = 1000
        client._last_word_end_ms["ctx-2"] = 1234

        await client._handle_timestamps(
            {
                "words": ["is", "your"],
                "start": [0.1, 0.2],
                "end": [0.2, 0.3],
            },
            "ctx-2",
        )
        words, _, text, _ = await client.get_words()

        assert text == " is your"
        assert [word.word for word in words] == [" ", "is", " ", "your"]

    asyncio.run(_run())


def test_build_request_payload_does_not_leak_auth_or_base_url():
    ten_env = MagicMock()
    config = CartesiaTTSConfig(
        params={
            "api_key": "secret",
            "base_url": "wss://example.invalid",
            "model_id": "sonic-3",
            "language": "en",
        }
    )
    config.update_params()
    client = CartesiaTTSClient(config=config, ten_env=ten_env)

    payload = client._build_request_payload("hello", "ctx-1")

    assert payload["transcript"] == "hello"
    assert payload["context_id"] == "ctx-1"
    assert payload["model_id"] == "sonic-3"
    assert payload["language"] == "en"
    assert "api_key" not in payload
    assert "base_url" not in payload


def test_cancel_cleans_last_word_end_ms():
    async def _run():
        ten_env = MagicMock()
        client = CartesiaTTSClient(
            config=CartesiaTTSConfig(api_key="test"),
            ten_env=ten_env,
        )
        client._last_word_end_ms["ctx-1"] = 1234

        await client.cancel("ctx-1")

        assert "ctx-1" not in client._last_word_end_ms

    asyncio.run(_run())


def test_process_audio_data_ignores_end_signal_from_old_request():
    async def _run():
        extension = CartesiaTTSExtension("cartesia_tts")
        extension.ten_env = MagicMock()
        extension.current_request_id = "new-request"
        extension.pending_audio_end = False
        extension.client = MagicMock()
        extension.client.get_audio = AsyncMock(
            side_effect=[
                (None, "old-request", 0),
                (None, "", 0),
            ]
        )
        extension._handle_completed_request = AsyncMock()
        extension._reset_tts_request_info = AsyncMock()

        await extension._process_audio_data()

        extension._handle_completed_request.assert_not_awaited()
        extension._reset_tts_request_info.assert_not_awaited()

    asyncio.run(_run())


def test_update_configs_redacts_api_key_in_logs():
    async def _run():
        extension = CartesiaTTSExtension("cartesia_tts")
        extension.ten_env = MagicMock()
        extension.config = CartesiaTTSConfig(api_key="existing")
        extension.config.update_params()
        extension.config_update_lock = asyncio.Lock()
        extension._apply_config_update = AsyncMock()

        await extension.update_configs(
            {"params": {"api_key": "super-secret", "model_id": "sonic-3"}}
        )

        logged_messages = [
            call.args[0] for call in extension.ten_env.log_info.call_args_list
        ]
        assert any("***" in message for message in logged_messages)
        assert all("super-secret" not in message for message in logged_messages)

    asyncio.run(_run())


def test_wait_for_client_available_times_out():
    async def _run():
        extension = CartesiaTTSExtension("cartesia_tts")
        extension.client = None
        extension._is_stopped = False

        result = await extension._wait_for_client_available(timeout_s=0.02)

        assert result is False

    asyncio.run(_run())


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


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_dump_functionality(MockCartesiaTTSClient):
    """Tests that the dump file from the TTS extension matches the audio received."""
    print("Starting test_dump_functionality with mock...")

    DUMP_PATH = "./dump/"
    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)
    os.makedirs(DUMP_PATH)

    fake_audio_chunk_1 = b"\x11\x22\x33\x44" * 20
    fake_audio_chunk_2 = b"\xaa\xbb\xcc\xdd" * 20

    mock_instance = _make_mock_client(
        audio_chunks_by_request={
            "tts_request_1": [
                (fake_audio_chunk_1, 0),
                (fake_audio_chunk_2, 0),
                (None, 0),  # end signal
            ],
        }
    )
    MockCartesiaTTSClient.return_value = mock_instance

    tester = ExtensionTesterDump()
    dump_config = {
        "dump": True,
        "dump_path": DUMP_PATH,
        "params": {"api_key": "test_api_key"},
    }
    tester.set_test_mode_single("cartesia_tts", json.dumps(dump_config))

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

    assert filecmp.cmp(
        tester.test_dump_file_path, tts_dump_file, shallow=False
    ), "Test dump file and TTS dump file should have the same content"

    print(
        f"✅ Dump functionality test passed: received {len(tester.received_audio_chunks)} audio chunks"
    )

    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)


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
        self.audio_end_reason = ""
        self.total_audio_duration_from_event = 0
        self.received_audio_bytes = 0
        self.sample_rate = 16000
        self.bytes_per_sample = 2
        self.channels = 1
        self.audio_received_after_flush_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("Flush test started, sending long TTS request.")
        tts_input = TTSTextInput(
            request_id="tts_request_for_flush",
            text="This is a very long text designed to generate a continuous stream of audio.",
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

        buf = audio_frame.lock_buf()
        try:
            self.received_audio_bytes += len(buf)
        finally:
            audio_frame.unlock_buf(buf)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "tts_audio_start":
            self.audio_start_received = True
            return

        json_str, _ = data.get_property_to_json(None)
        if not json_str:
            return
        payload = json.loads(json_str)

        if name == "tts_flush_start":
            self.flush_start_received = True
        elif name == "tts_audio_end":
            self.audio_end_received = True
            self.audio_end_reason = payload.get("reason")
            self.total_audio_duration_from_event = payload.get(
                "request_total_audio_duration_ms"
            )
        elif name == "tts_flush_end":
            self.flush_end_received = True

            def stop_test_later():
                ten_env.log_info("Waited after flush_end, stopping test now.")
                ten_env.stop_test()

            timer = threading.Timer(0.5, stop_test_later)
            timer.start()

    def get_calculated_audio_duration_ms(self) -> int:
        duration_sec = self.received_audio_bytes / (
            self.sample_rate * self.bytes_per_sample * self.channels
        )
        return int(duration_sec * 1000)


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_flush_logic(MockCartesiaTTSClient):
    """Tests that sending a flush command during TTS streaming correctly stops the audio."""
    print("Starting test_flush_logic with mock...")

    # Build a mock that streams many audio chunks slowly
    mock_instance = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.set_current_request_id = AsyncMock()
    mock_instance.send_audio_end_signal = AsyncMock()

    # When text_to_speech is called, schedule slow audio streaming
    async def mock_tts(t):
        request_id = t.request_id

        async def _stream():
            for _ in range(20):
                if mock_instance.stop.called:
                    return
                await pcm_queue.put((b"\x11\x22\x33" * 100, request_id, 0))
                await asyncio.sleep(0.1)
            # Normal end
            await pcm_queue.put((None, request_id, 0))

        asyncio.create_task(_stream())

    mock_instance.text_to_speech = AsyncMock(side_effect=mock_tts)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock_instance.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock_instance.get_words = AsyncMock(side_effect=mock_get_words)

    MockCartesiaTTSClient.return_value = mock_instance

    config = {"params": {"api_key": "test_api_key"}}
    tester = ExtensionTesterFlush()
    tester.set_test_mode_single("cartesia_tts", json.dumps(config))

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

    print("✅ Flush logic test passed successfully.")


# ================ test SSML ================
class ExtensionTesterSSML(ExtensionTester):
    def __init__(self, metadata=None):
        super().__init__()
        self.metadata = metadata or {}
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="ssml_request",
            text="TEN loves 1234",
            text_input_end=True,
            metadata=self.metadata,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_ssml_presets(MockCartesiaTTSClient):
    """Ensure SSML presets from configuration are applied."""

    captured_texts = []

    mock_instance = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.set_current_request_id = AsyncMock()
    mock_instance.send_audio_end_signal = AsyncMock()

    async def mock_tts(t):
        captured_texts.append(t.text)
        request_id = t.request_id

        async def _stream():
            await asyncio.sleep(0.01)
            await pcm_queue.put((None, request_id, 0))

        asyncio.create_task(_stream())

    mock_instance.text_to_speech = AsyncMock(side_effect=mock_tts)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock_instance.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock_instance.get_words = AsyncMock(side_effect=mock_get_words)

    MockCartesiaTTSClient.return_value = mock_instance

    tester = ExtensionTesterSSML()
    config = {
        "params": {"api_key": "test_key"},
        "ssml": {
            "enabled": True,
            "speed_ratio": 1.2,
            "volume_ratio": 0.8,
            "emotion": "happy",
            "post_break_time": "500ms",
            "spell_words": ["TEN"],
        },
    }

    tester.set_test_mode_single("cartesia_tts", json.dumps(config))
    tester.run()

    assert (
        tester.audio_end_received
    ), "Expected SSML test to finish with audio end"
    assert len(captured_texts) > 0, "Expected Cartesia client to receive text"
    captured_text = captured_texts[0]
    assert captured_text.startswith('<speed ratio="1.2"/>'), captured_text
    assert '<volume ratio="0.8"/>' in captured_text
    assert '<emotion value="happy"/>' in captured_text
    assert "<spell>TEN</spell>" in captured_text
    assert captured_text.endswith('<break time="500ms"/>'), captured_text


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_ssml_metadata_overrides(MockCartesiaTTSClient):
    """Metadata overrides should clamp ratios and honour breaks."""

    captured_texts = []

    mock_instance = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.set_current_request_id = AsyncMock()
    mock_instance.send_audio_end_signal = AsyncMock()

    async def mock_tts(t):
        captured_texts.append(t.text)
        request_id = t.request_id

        async def _stream():
            await asyncio.sleep(0.01)
            await pcm_queue.put((None, request_id, 0))

        asyncio.create_task(_stream())

    mock_instance.text_to_speech = AsyncMock(side_effect=mock_tts)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock_instance.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock_instance.get_words = AsyncMock(side_effect=mock_get_words)

    MockCartesiaTTSClient.return_value = mock_instance

    metadata = {
        "ssml": {
            "enabled": True,
            "speed_ratio": 5.0,
            "volume_ratio": 0.1,
            "emotion": "sad",
            "pre_break_time": "1s",
            "spell_words": ["1234"],
        }
    }

    tester = ExtensionTesterSSML(metadata=metadata)
    config = {
        "params": {"api_key": "test_key"},
        "ssml": {"enabled": False},
    }

    tester.set_test_mode_single("cartesia_tts", json.dumps(config))
    tester.run()

    assert (
        tester.audio_end_received
    ), "Expected metadata override test to finish"
    assert len(captured_texts) > 0, "Expected metadata override to send text"
    captured_text = captured_texts[0]
    assert captured_text.startswith('<break time="1s"/>'), captured_text
    assert '<speed ratio="1.5"/>' in captured_text
    assert '<volume ratio="0.5"/>' in captured_text
    assert '<emotion value="sad"/>' in captured_text
    assert "<spell>1234</spell>" in captured_text
