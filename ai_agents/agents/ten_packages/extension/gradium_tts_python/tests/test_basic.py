import asyncio
import filecmp
import json
import os
import shutil
import tempfile
from unittest.mock import MagicMock, patch

from ten_ai_base.struct import TTSFlush, TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.extension import GradiumTTSExtension

from .gradium_mocks import make_streaming_mock_client

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

    mock_client.return_value = make_streaming_mock_client(
        audio_chunks=(b"\x11\x22\x33\x44" * 20, b"\xaa\xbb\xcc\xdd" * 20),
        ttfb_ms=255,
        extra_metadata={"voice_id": "cLONiZ4hQ8VpQ4Sz"},
    )

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
    mock_client.return_value = make_streaming_mock_client(
        audio_chunks=(b"\x00\x01\x02\x03" * 100,),
        ttfb_ms=150,
    )

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
    mock_client.return_value = make_streaming_mock_client(ttfb_ms=80)

    tester = ExtensionTesterFlush()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received


class ExtensionTesterSegments(ExtensionTester):
    """Send several segments under one request_id, end with text_input_end."""

    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for text, text_input_end in [
            ("Hello world, this is the first sentence.", False),
            (" This is the second sentence.", False),
            (" And the third.", True),
        ]:
            payload = TTSTextInput(
                request_id="tts_request_segments",
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
def test_segments_are_forwarded_immediately(mock_client):
    """Each non-empty segment is forwarded to the vendor as it arrives,
    in order, over a single session; the empty text_input_end finalizes."""
    sent_texts = []
    mock_client.return_value = make_streaming_mock_client(sent_texts=sent_texts)

    tester = ExtensionTesterSegments()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received
    assert sent_texts == [
        "Hello world, this is the first sentence.",
        " This is the second sentence.",
        " And the third.",
    ]


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_single_session_per_request(mock_client):
    """All segments of one request share a single streaming session."""
    mock_instance = make_streaming_mock_client()
    mock_client.return_value = mock_instance

    tester = ExtensionTesterSegments()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.audio_end_received
    # One session opened and ended for the whole request, not one per segment.
    assert mock_instance.start_session.await_count == 1
    assert mock_instance.end_input.await_count == 1
    assert mock_instance.send_text.await_count == 3


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
