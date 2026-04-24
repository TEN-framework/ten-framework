import asyncio
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.config import XAIASRConfig
from xai_asr_python.extension import XAIASRExtension


def test_finalize_emits_result_and_finalize_end():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.config = XAIASRConfig(
            finalize_timeout_ms=10,
            params={"api_key": "xai-test-key"},
        )
        extension.recognition = MagicMock()
        extension.recognition.send_audio_done = AsyncMock()
        extension.recognition.wait_for_done = AsyncMock(
            return_value={"text": "done text", "start": 0.1, "duration": 0.2}
        )
        extension.recognition.done_event = asyncio.Event()
        extension._emit_asr_result = AsyncMock()
        extension.send_asr_finalize_end = AsyncMock()

        await extension.finalize("session-123")

        assert extension.last_finalize_timestamp == 0
        extension.recognition.send_audio_done.assert_awaited_once()
        extension._emit_asr_result.assert_awaited_once()
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())


def test_finalize_timeout_still_emits_finalize_end():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.config = XAIASRConfig(
            finalize_timeout_ms=10,
            params={"api_key": "xai-test-key"},
        )
        extension.recognition = MagicMock()
        extension.recognition.send_audio_done = AsyncMock()
        extension.recognition.wait_for_done = AsyncMock(return_value=None)
        extension.recognition.done_event = asyncio.Event()
        extension._emit_asr_result = AsyncMock()
        extension.send_asr_finalize_end = AsyncMock()

        await extension.finalize("session-123")

        assert extension.last_finalize_timestamp == 0
        extension.recognition.send_audio_done.assert_awaited_once()
        extension._emit_asr_result.assert_not_awaited()
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())


def test_finalize_when_disconnected_emits_finalize_end_without_waiting():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.config = XAIASRConfig(
            finalize_timeout_ms=10,
            params={"api_key": "xai-test-key"},
        )
        extension.recognition = MagicMock()
        extension.recognition.is_connected.return_value = False
        extension.send_asr_finalize_end = AsyncMock()

        await extension.finalize("session-123")

        assert extension.last_finalize_timestamp == 0
        extension.send_asr_finalize_end.assert_awaited_once()
        extension.recognition.send_audio_done.assert_not_called()

    asyncio.run(_run())
