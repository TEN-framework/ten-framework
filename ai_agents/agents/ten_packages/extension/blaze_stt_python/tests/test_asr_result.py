#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock

from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension


class FakeTimeline:
    def get_audio_duration_before_time(self, value: int) -> int:
        return value


def _extension(params: dict | None = None) -> BlazeSTTExtension:
    extension = BlazeSTTExtension("blaze_stt_python")
    extension.ten_env = MagicMock()
    extension.audio_timeline = FakeTimeline()
    extension.config = BlazeSTTConfig(
        params={"api_key": "test-key", "language": "vi", **(params or {})}
    )
    extension.config.apply_defaults()
    extension.send_asr_result = AsyncMock()
    return extension


def _last_result(extension):
    return extension.send_asr_result.await_args.args[0]


def test_final_result_shape():
    async def _run():
        extension = _extension()
        extension.metadata = {"session_id": "session-123", "turn_id": 4}

        await extension.on_final_result(
            {
                "type": "final",
                "text": " xin chào.",
                "start_ms": 120,
                "end_ms": 460,
            }
        )

        result = _last_result(extension)
        assert result.text == "xin chào."
        assert result.final is True
        assert result.start_ms == 120
        assert result.duration_ms == 340
        assert result.language == "vi-VN"
        assert result.metadata["session_id"] == "session-123"
        assert "session_id" not in result.metadata["asr_info"]
        assert result.metadata["asr_info"]["vendor"] == "blaze"
        assert result.metadata["asr_info"]["turn_id"] == 4

    asyncio.run(_run())


def test_partial_result_is_interim_and_starts_after_last_final():
    async def _run():
        extension = _extension()
        extension.metadata = {"session_id": "s"}
        await extension.on_final_result(
            {"text": "một.", "start_ms": 0, "end_ms": 700}
        )

        await extension.on_partial_result({"type": "partial", "text": "hai"})

        result = _last_result(extension)
        assert result.final is False
        assert result.start_ms == 700
        assert result.duration_ms == 0
        assert result.metadata["asr_info"]["locked"] is False
        assert extension._has_pending_speech is True

    asyncio.run(_run())


def test_empty_results_are_not_emitted():
    async def _run():
        extension = _extension()
        extension.metadata = {"session_id": "s"}
        await extension.on_partial_result({"text": "  "})
        await extension.on_final_result(
            {"text": "", "start_ms": 0, "end_ms": 0}
        )
        extension.send_asr_result.assert_not_awaited()

    asyncio.run(_run())


def test_metadata_is_not_mutated():
    async def _run():
        extension = _extension()
        extension.metadata = {"session_id": "session-123"}
        await extension.on_final_result({"text": "hello", "start_ms": 0})
        assert extension.metadata == {"session_id": "session-123"}

    asyncio.run(_run())


def test_offsets_continue_after_reconnect():
    async def _run():
        extension = _extension()
        extension.metadata = {"session_id": "s"}
        extension.sent_user_audio_duration_ms_before_last_reset = 5000

        await extension.on_final_result(
            {"text": "hello", "start_ms": 200, "end_ms": 400}
        )

        result = _last_result(extension)
        assert result.start_ms == 5200
        assert result.duration_ms == 200

    asyncio.run(_run())


def test_result_language_override():
    async def _run():
        extension = _extension({"result_language_default": "vi"})
        extension.metadata = {"session_id": "s"}
        await extension.on_final_result({"text": "hello", "start_ms": 0})
        assert _last_result(extension).language == "vi"

    asyncio.run(_run())
