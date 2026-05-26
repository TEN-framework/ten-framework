import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.message import (
    ModuleErrorCode,
    ModuleType,
    TTSAudioEndReason,
)
from xai_tts_python.config import XAITTSConfig
from xai_tts_python.extension import XAITTSExtension
from xai_tts_python.xai_tts import XAITTSConnectionException


def _make_extension():
    extension = XAITTSExtension("xai_tts_python")
    extension.config = XAITTSConfig(api_key="xai-test-key", dump=False)
    extension.ten_env = MagicMock()
    extension.current_request_id = "req-1"
    extension.send_tts_audio_start = AsyncMock()
    extension.send_tts_audio_end = AsyncMock()
    extension.send_usage_metrics = AsyncMock()
    extension.send_tts_error = AsyncMock()
    extension.finish_request = AsyncMock()
    extension._audio_start_sent = True
    return extension


def test_handle_connection_error_marks_403_as_fatal():
    async def _run():
        extension = _make_extension()
        exc = XAITTSConnectionException(status_code=403, body="forbidden")

        await extension._handle_connection_error(exc, text_input_end=True)

        end_call = extension.send_tts_audio_end.await_args
        finish_call = extension.finish_request.await_args
        assert finish_call is not None, "finish_request should run on finalize"
        assert end_call.kwargs["reason"] == TTSAudioEndReason.ERROR
        error = finish_call.kwargs["error"]
        assert error is not None
        assert error.code == int(ModuleErrorCode.FATAL_ERROR.value)
        assert error.module == ModuleType.TTS
        assert error.vendor_info is not None
        assert error.vendor_info.code == "403"

    asyncio.run(_run())


def test_handle_connection_error_marks_500_as_non_fatal():
    async def _run():
        extension = _make_extension()
        exc = XAITTSConnectionException(status_code=500, body="server error")

        await extension._handle_connection_error(exc, text_input_end=True)

        finish_call = extension.finish_request.await_args
        error = finish_call.kwargs["error"]
        assert error.code == int(ModuleErrorCode.NON_FATAL_ERROR.value)

    asyncio.run(_run())


def test_finalize_request_pops_recorder_from_map():
    async def _run():
        extension = _make_extension()
        recorder = MagicMock()
        recorder.flush = AsyncMock()
        extension.recorder_map["req-1"] = recorder

        await extension._finalize_request(TTSAudioEndReason.REQUEST_END)

        recorder.flush.assert_awaited_once()
        assert (
            "req-1" not in extension.recorder_map
        ), "recorder_map should not retain entries after finalize"

    asyncio.run(_run())


def test_finalize_request_clears_recorder_even_when_flush_fails():
    async def _run():
        extension = _make_extension()
        recorder = MagicMock()
        recorder.flush = AsyncMock(side_effect=RuntimeError("disk full"))
        extension.recorder_map["req-1"] = recorder

        await extension._finalize_request(TTSAudioEndReason.REQUEST_END)

        assert "req-1" not in extension.recorder_map

    asyncio.run(_run())
