"""The finite-audio collector must count actual PCM, including split samples."""

import asyncio
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock

import pytest

from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.struct import TTSTextInput


@pytest.mark.parametrize("sample_rate", [16000, 24000])
@pytest.mark.parametrize("reason", list(TTSAudioEndReason))
def test_pcm_byte_receipt_matches_frames_and_resets_per_request(
    sample_rate, reason
):
    asyncio.run(check_pcm_receipt(sample_rate, reason))


async def check_pcm_receipt(sample_rate, reason):
    from elevenlabs_tts2_python.extension import ElevenLabsTTS2Extension

    extension = ElevenLabsTTS2Extension("tts")
    extension.config = SimpleNamespace(sample_rate=sample_rate, dump=False)
    extension.ten_env = Mock(
        send_data=AsyncMock(), send_audio_frame=AsyncMock()
    )
    extension.vendor_metadata = lambda: {}
    extension.client = Mock(
        send_text=AsyncMock(),
        synthesizer=SimpleNamespace(send_text_in_connection=False),
    )

    for request_id, chunks in (("first", [b"a", b"bcd"]), ("second", [])):
        extension.metadatas[request_id] = {"request_id": request_id}
        await extension.request_tts(
            TTSTextInput(
                request_id=request_id, text="Read", text_input_end=True
            )
        )
        if chunks:
            await extension.send_tts_audio_start(request_id)
        emitted = bytearray()
        for chunk in chunks:
            await extension.send_tts_audio_data(chunk)
        for call in extension.ten_env.send_audio_frame.await_args_list:
            frame = call.args[0]
            buffer = frame.lock_buf()
            emitted.extend(buffer)
            frame.unlock_buf(buffer)
        await extension.send_tts_audio_end(
            request_id,
            0,
            len(emitted) * 1000 / (sample_rate * 2),
            reason=reason,
        )
        event = extension.ten_env.send_data.await_args.args[0]
        payload, _ = event.get_property_to_json(None)
        payload = json.loads(payload)
        assert payload["request_id"] == request_id
        assert payload["reason"] == reason.value
        assert payload["metadata"]["pcm_bytes"] == len(emitted)
        assert emitted == (b"abcd" if chunks else b"")
        extension.ten_env.send_audio_frame.reset_mock()
