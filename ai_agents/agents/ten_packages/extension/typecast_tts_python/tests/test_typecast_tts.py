import sys
from pathlib import Path
import asyncio
import json
import threading
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, TenError
from ten_runtime import TenErrorCode
from ten_ai_base.struct import TTS2HttpResponseEventType, TTSTextInput
from ten_ai_base.helper import PCMWriter
from ten_ai_base.helper import write_pcm_to_file
from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.tts2_http import AsyncTTS2HttpExtension

from pcm import StreamingWavToPcm16
from typecast_tts_python.config import TypecastTTSConfig
from typecast_tts_python.extension import TypecastTTSExtension
from typecast_tts_python.typecast_tts import TypecastTTSClient
from typecast import TypecastError, UnauthorizedError


def test_config_defaults_and_forces_wav():
    config = TypecastTTSConfig(
        params={
            "api_key": "key",
            "voice_id": "voice",
            "url": "https://example.com/",
            "output": {"audio_format": "mp3", "audio_tempo": 1.1},
        }
    )

    config.update_params()
    config.validate()

    assert config.url == "https://example.com"
    assert "url" not in config.params
    assert config.params["model"] == "ssfm-v30"
    assert config.params["output"] == {
        "audio_format": "wav",
        "audio_tempo": 1.1,
    }


@pytest.mark.parametrize("missing", ["api_key", "voice_id"])
def test_config_requires_credentials_and_voice(missing):
    params = {"api_key": "key", "voice_id": "voice"}
    params[missing] = ""
    config = TypecastTTSConfig(params=params)
    config.update_params()

    with pytest.raises(ValueError, match=missing):
        config.validate()


def test_streaming_wav_to_pcm16_strips_header_across_chunks():
    converter = StreamingWavToPcm16()
    header = b"h" * 44

    assert converter.feed(header[:20]) == b""
    assert converter.feed(header[20:] + b"\x01\x02\x03") == b"\x01\x02"
    assert converter.feed(b"\x04\x05") == b"\x03\x04"
    assert converter.feed(b"\x06") == b"\x05\x06"


def test_streaming_wav_to_pcm16_strips_header_in_single_chunk():
    converter = StreamingWavToPcm16()
    header = b"h" * 44

    assert converter.feed(header + b"\x01\x02\x03\x04") == b"\x01\x02\x03\x04"


def test_extension_flushes_dump_tail_added_during_write(tmp_path):
    output = tmp_path / "dump.pcm"
    first_write_started = threading.Event()
    release_first_write = threading.Event()
    write_count = 0

    def delayed_write(buffer, file_name):
        nonlocal write_count
        write_count += 1
        if write_count == 1:
            first_write_started.set()
            release_first_write.wait()
        write_pcm_to_file(buffer, file_name)

    async def run():
        writer = PCMWriter(str(output), buffer_size=4)
        extension = TypecastTTSExtension("test")
        extension.recorder_map = {"request-id": writer}

        async def base_finish(self, request_id, reason, log_message=None):
            await self.recorder_map[request_id].flush()

        with patch(
            "ten_ai_base.helper.write_pcm_to_file",
            side_effect=delayed_write,
        ), patch.object(
            AsyncTTS2HttpExtension,
            "_send_audio_end_and_finish",
            base_finish,
        ):
            await writer.write(b"head")
            while not first_write_started.is_set():
                await asyncio.sleep(0)
            await writer.write(b"tail")
            flush_task = asyncio.create_task(
                extension._send_audio_end_and_finish(
                    request_id="request-id",
                    reason=TTSAudioEndReason.REQUEST_END,
                )
            )
            await asyncio.sleep(0)
            release_first_write.set()
            await flush_task

    asyncio.run(run())

    assert output.read_bytes() == b"headtail"


def test_client_empty_text_ends_without_request():
    config = TypecastTTSConfig(params={"api_key": "key", "voice_id": "voice"})
    config.update_params()
    client = TypecastTTSClient(config, MagicMock())

    async def collect():
        return [event async for event in client.get("  ", "request-id")]

    assert asyncio.run(collect()) == [(None, TTS2HttpResponseEventType.END)]


@pytest.mark.parametrize(
    ("error", "expected_event"),
    [
        (
            UnauthorizedError("invalid key"),
            TTS2HttpResponseEventType.INVALID_KEY_ERROR,
        ),
        (TypecastError("rate limited", 429), TTS2HttpResponseEventType.ERROR),
    ],
)
def test_client_maps_vendor_errors(error, expected_event):
    config = TypecastTTSConfig(params={"api_key": "key", "voice_id": "voice"})
    config.update_params()
    client = TypecastTTSClient(config, MagicMock())

    async def failing_stream(request, chunk_size):
        raise error
        yield b""  # pragma: no cover

    mock_sdk = MagicMock()
    mock_sdk.__aenter__ = AsyncMock(return_value=mock_sdk)
    mock_sdk.__aexit__ = AsyncMock(return_value=None)
    mock_sdk.text_to_speech_stream = failing_stream

    async def collect():
        with patch(
            "typecast_tts_python.typecast_tts.AsyncTypecast",
            return_value=mock_sdk,
        ):
            return [event async for event in client.get("hello", "request-id")]

    events = asyncio.run(collect())
    assert events == [(str(error).encode(), expected_event)]


class TypecastTTSExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello typecast",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        await ten_env.send_data(data)
        asyncio.create_task(self._stop_on_timeout(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        if data.get_name() == "tts_audio_end":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            assert data_dict["request_id"] == "tts_request_1"
            self.audio_end_received = True
            ten_env.stop_test()

    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame):
        buf = audio_frame.lock_buf()
        try:
            self.received_audio_chunks.append(bytes(buf))
        finally:
            audio_frame.unlock_buf(buf)

    async def _stop_on_timeout(self, ten_env: AsyncTenEnvTester) -> None:
        await asyncio.sleep(10)
        ten_env.stop_test(
            TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message="test timeout",
            )
        )


def test_typecast_tts_extension_success():
    wav_header = b"h" * 44
    audio_chunk_1 = b"\x01\x02\x03\x04"
    audio_chunk_2 = b"\x05\x06\x07\x08"

    async def mock_text_to_speech_stream(request, chunk_size):
        assert request.text == "hello typecast"
        assert request.output.audio_format == "wav"
        assert chunk_size == 8192
        yield wav_header[:20]
        yield wav_header[20:] + audio_chunk_1
        yield audio_chunk_2

    with patch("typecast_tts_python.typecast_tts.AsyncTypecast") as mock_cls:
        mock_client = MagicMock()
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=None)
        mock_client.text_to_speech_stream = mock_text_to_speech_stream
        mock_cls.return_value = mock_client

        property_json = {
            "params": {
                "api_key": "test_api_key",
                "voice_id": "test_voice_id",
                "model": "ssfm-v30",
            }
        }

        tester = TypecastTTSExtensionTester()
        tester.set_test_mode_single(
            "typecast_tts_python", json.dumps(property_json)
        )

        err = tester.run()

    assert err is None, (
        "test_typecast_tts_extension_success err: "
        f"{err.error_message() if err else 'None'}"
    )
    assert tester.audio_end_received
    assert b"".join(tester.received_audio_chunks) == (
        audio_chunk_1 + audio_chunk_2
    )
    mock_cls.assert_called_once_with(
        host="https://api.typecast.ai",
        api_key="test_api_key",
    )
    mock_client.__aexit__.assert_awaited_once_with(None, None, None)


if __name__ == "__main__":
    test_streaming_wav_to_pcm16_strips_header_across_chunks()
    test_streaming_wav_to_pcm16_strips_header_in_single_chunk()
