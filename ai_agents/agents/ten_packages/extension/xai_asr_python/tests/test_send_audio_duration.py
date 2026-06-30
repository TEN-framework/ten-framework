import asyncio
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.recognition import (
    XAIASRRecognition,
    XAIASRRecognitionCallback,
)


def _make_recognition(config):
    callback = MagicMock(spec=XAIASRRecognitionCallback)
    recognition = XAIASRRecognition(
        api_key="xai-test-key",
        audio_timeline=MagicMock(),
        ten_env=MagicMock(),
        config=config,
        callback=callback,
    )
    recognition.websocket = MagicMock()
    recognition.websocket.send = AsyncMock()
    recognition.is_started = True
    recognition.is_connected = MagicMock(return_value=True)
    recognition.ready_event.set()
    return recognition


def test_send_audio_frame_pcm_duration_uses_2_bytes_per_sample():
    async def _run():
        recognition = _make_recognition(
            {"sample_rate": 16000, "encoding": "pcm", "channels": 1}
        )
        # 16000 Hz, 2 bytes/sample, mono → 32 bytes/ms.
        # 320 bytes → 10 ms.
        await recognition.send_audio_frame(b"\x00" * 320)
        recognition.audio_timeline.add_user_audio.assert_called_once_with(10)

    asyncio.run(_run())


def test_send_audio_frame_mulaw_duration_uses_1_byte_per_sample():
    async def _run():
        recognition = _make_recognition(
            {"sample_rate": 8000, "encoding": "mulaw", "channels": 1}
        )
        # 8000 Hz, 1 byte/sample, mono → 8 bytes/ms.
        # 320 bytes → 40 ms.
        await recognition.send_audio_frame(b"\x00" * 320)
        recognition.audio_timeline.add_user_audio.assert_called_once_with(40)

    asyncio.run(_run())


def test_send_audio_frame_pcm_stereo_duration_includes_channels():
    async def _run():
        recognition = _make_recognition(
            {"sample_rate": 16000, "encoding": "pcm", "channels": 2}
        )
        # 16000 Hz, 2 bytes/sample, 2 channels → 64 bytes/ms.
        # 640 bytes → 10 ms.
        await recognition.send_audio_frame(b"\x00" * 640)
        recognition.audio_timeline.add_user_audio.assert_called_once_with(10)

    asyncio.run(_run())
