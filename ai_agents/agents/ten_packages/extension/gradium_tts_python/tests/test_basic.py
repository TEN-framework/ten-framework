import asyncio
import filecmp
import json
import os
import shutil
import tempfile
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import TTSFlush, TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.extension import GradiumTTSExtension
from gradium_tts_python.gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)

MOCK_CONFIG = {
    "params": {
        "api_key": "test_api_key",
        "voice_id": "cLONiZ4hQ8VpQ4Sz",
        "sample_rate": 24000,
    }
}


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
        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello gradium",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, _ten_env: TenEnvTester, audio_frame):
        buf = audio_frame.lock_buf()
        try:
            self.received_audio_chunks.append(bytes(buf))
        finally:
            audio_frame.unlock_buf(buf)

    def write_test_dump_file(self):
        with open(self.test_dump_file_path, "wb") as file:
            for chunk in self.received_audio_chunks:
                file.write(chunk)

    def find_tts_dump_file(self) -> str | None:
        if not os.path.exists(self.dump_dir):
            return None
        for filename in os.listdir(self.dump_dir):
            if filename.endswith(".pcm") and filename != os.path.basename(
                self.test_dump_file_path
            ):
                return os.path.join(self.dump_dir, filename)
        return None


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_dump_functionality(mock_client):
    dump_path = "./dump/"
    if os.path.exists(dump_path):
        shutil.rmtree(dump_path)
    os.makedirs(dump_path)

    mock_instance = mock_client.return_value
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {
        "voice_id": "cLONiZ4hQ8VpQ4Sz"
    }

    fake_audio_chunk_1 = b"\x11\x22\x33\x44" * 20
    fake_audio_chunk_2 = b"\xaa\xbb\xcc\xdd" * 20

    async def mock_get_audio_stream(_text: str, *_args):
        yield 255, EVENT_TTS_TTFB_METRIC
        yield fake_audio_chunk_1, EVENT_TTS_RESPONSE
        await asyncio.sleep(0.01)
        yield fake_audio_chunk_2, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterDump()
    dump_config = {
        "dump": True,
        "dump_path": dump_path,
        **MOCK_CONFIG,
    }
    tester.set_test_mode_single("gradium_tts_python", json.dumps(dump_config))
    tester.run()

    assert tester.audio_end_received
    assert tester.received_audio_chunks

    tester.write_test_dump_file()
    tts_dump_file = tester.find_tts_dump_file()
    assert tts_dump_file is not None
    assert os.path.exists(tts_dump_file)
    assert filecmp.cmp(
        tester.test_dump_file_path,
        tts_dump_file,
        shallow=False,
    )

    shutil.rmtree(dump_path)


class ExtensionTesterBasic(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_received = False
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="tts_request_basic",
            text="Hello, this is a test of the Gradium TTS extension.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_start":
            self.audio_start_received = True
        elif name == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, _ten_env: TenEnvTester, _audio_frame):
        self.audio_chunks_count += 1


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_basic_audio(mock_client):
    mock_instance = mock_client.return_value
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {}

    fake_audio_chunk = b"\x00\x01\x02\x03" * 100

    async def mock_get_audio_stream(_text: str, *_args):
        yield 150, EVENT_TTS_TTFB_METRIC
        yield fake_audio_chunk, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_start_received
    assert tester.audio_end_received
    assert tester.audio_chunks_count > 0


class ExtensionTesterFlush(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
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
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_flush(mock_client):
    mock_instance = mock_client.return_value
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {}

    async def mock_get_audio_stream(_text: str, *_args):
        yield 80, EVENT_TTS_TTFB_METRIC
        yield b"\x00\x01" * 100, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream

    tester = ExtensionTesterFlush()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received


class ExtensionTesterBufferedFragments(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for text, text_input_end in [
            ("Sure!", False),
            (" Here you go:\n\n1,", False),
            (" 2,", False),
            (" 3,", False),
            (" 10.", False),
            ("", True),
        ]:
            payload = TTSTextInput(
                request_id="tts_request_buffered",
                text=text,
                text_input_end=text_input_end,
                metadata={"session_id": "s", "turn_id": 1},
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, payload.model_dump_json())
            ten_env_tester.send_data(data)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_comma_delimited_fragments_are_buffered(mock_client):
    sent_texts = []
    mock_instance = MagicMock()
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {}

    async def mock_get_audio_stream(text: str, *_args):
        sent_texts.append(text)
        yield 175, EVENT_TTS_TTFB_METRIC
        yield b"\x00\x01" * 100, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream
    mock_client.return_value = mock_instance

    tester = ExtensionTesterBufferedFragments()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received
    assert sent_texts == ["Sure! Here you go:\n\n1, 2, 3, 10."]


class ExtensionTesterAppendInputGrouping(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for text, text_input_end in [
            ("Hello world, this is the first text input.", False),
            (" This is the second text input for testing.", False),
            ("", True),
        ]:
            payload = TTSTextInput(
                request_id="tts_request_append_grouped",
                text=text,
                text_input_end=text_input_end,
                metadata={"session_id": "s", "turn_id": 1},
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, payload.model_dump_json())
            ten_env_tester.send_data(data)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_short_append_input_group_uses_single_vendor_request(mock_client):
    sent_texts = []
    mock_instance = MagicMock()
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {}

    async def mock_get_audio_stream(text: str, *_args):
        sent_texts.append(text)
        yield 125, EVENT_TTS_TTFB_METRIC
        yield b"\x00\x01" * 100, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream
    mock_client.return_value = mock_instance

    tester = ExtensionTesterAppendInputGrouping()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received
    assert sent_texts == [
        "Hello world, this is the first text input. This is the second text input for testing.",
    ]


class ExtensionTesterInterleavedGrouping(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for request_id, text, text_input_end in [
            ("interleave_req_1", "Request one starts with a list:", False),
            ("interleave_req_2", "Request two starts now and stays separate.", False),
            ("interleave_req_1", " 1, 2, 3, and finally ends here.", True),
            ("interleave_req_2", " It also finishes in one vendor request.", True),
        ]:
            payload = TTSTextInput(
                request_id=request_id,
                text=text,
                text_input_end=text_input_end,
                metadata={"session_id": "s", "turn_id": 1},
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, payload.model_dump_json())
            ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            request_id, _ = data.get_property_string("request_id")
            self.audio_end_received.append(request_id)
            if len(self.audio_end_received) == 2:
                ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_interleaved_requests_keep_separate_buffers(mock_client):
    sent_texts = []
    mock_instance = MagicMock()
    mock_instance.start = AsyncMock()
    mock_instance.clean = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.get_ready_sample_rate.return_value = 24000
    mock_instance.get_extra_metadata.return_value = {}

    async def mock_get_audio_stream(text: str, request_id: str, *_args):
        sent_texts.append((request_id, text))
        yield 125, EVENT_TTS_TTFB_METRIC
        yield b"\x00\x01" * 100, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock_instance.get.side_effect = mock_get_audio_stream
    mock_client.return_value = mock_instance

    tester = ExtensionTesterInterleavedGrouping()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received == ["interleave_req_1", "interleave_req_2"]
    assert sent_texts == [
        (
            "interleave_req_1",
            "Request one starts with a list: 1, 2, 3, and finally ends here.",
        ),
        (
            "interleave_req_2",
            "Request two starts now and stays separate. It also finishes in one vendor request.",
        ),
    ]


@patch("gradium_tts_python.extension.PCMWriter")
def test_setup_recorder_creates_dump_directory(mock_pcm_writer):
    async def _run():
        extension = GradiumTTSExtension("gradium_tts_python")
        extension.ten_env = MagicMock()
        base_dir = tempfile.mkdtemp(prefix="gradium-tts-dump-")
        dump_dir = os.path.join(base_dir, "nested", "dump")

        try:
            extension.config = GradiumTTSConfig(
                api_key="test_api_key",
                voice_id="cLONiZ4hQ8VpQ4Sz",
                dump=True,
                dump_path=dump_dir,
            )
            await extension._setup_recorder("req-1")
            assert os.path.isdir(dump_dir)
            mock_pcm_writer.assert_called_once_with(
                os.path.join(dump_dir, "gradium_dump_req-1.pcm")
            )
        finally:
            shutil.rmtree(base_dir)

    asyncio.run(_run())
