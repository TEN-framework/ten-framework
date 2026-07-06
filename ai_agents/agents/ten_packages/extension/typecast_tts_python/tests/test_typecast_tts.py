import sys
from pathlib import Path
import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, TenError
from ten_runtime import TenErrorCode
from ten_ai_base.struct import TTSTextInput

from pcm import StreamingWavToPcm16


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


if __name__ == "__main__":
    test_streaming_wav_to_pcm16_strips_header_across_chunks()
    test_streaming_wav_to_pcm16_strips_header_in_single_chunk()
