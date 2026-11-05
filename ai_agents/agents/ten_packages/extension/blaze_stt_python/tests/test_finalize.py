#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock

from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension


def _extension(timeout_ms: int = 2000) -> BlazeSTTExtension:
    extension = BlazeSTTExtension("blaze_stt_python")
    extension.ten_env = MagicMock()
    extension.config = BlazeSTTConfig(
        finalize_timeout_ms=timeout_ms, params={"api_key": "k"}
    )
    extension.config.apply_defaults()
    extension.send_asr_finalize_end = AsyncMock()
    return extension


def _recognition(final_arrives: bool):
    rec = MagicMock()
    rec.is_connected.return_value = True
    rec.final_event = asyncio.Event()
    rec.send_silence = AsyncMock()
    rec.wait_for_final = AsyncMock(return_value=final_arrives)
    return rec


def test_finalize_without_connection_ends_immediately():
    async def _run():
        extension = _extension()
        extension.recognition = None
        await extension.finalize("s")
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())


def test_finalize_without_pending_speech_ends_immediately():
    async def _run():
        extension = _extension()
        extension.recognition = _recognition(final_arrives=True)
        extension._has_pending_speech = False

        await extension.finalize("s")

        extension.recognition.send_silence.assert_not_awaited()
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())


def test_finalize_sends_silence_and_waits_for_final():
    async def _run():
        extension = _extension()
        extension.recognition = _recognition(final_arrives=True)
        extension._has_pending_speech = True

        await extension.finalize("s")

        extension.recognition.send_silence.assert_awaited_once_with(800)
        extension.recognition.wait_for_final.assert_awaited_once_with(2000)
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())


def test_finalize_timeout_still_ends_once():
    async def _run():
        extension = _extension(timeout_ms=50)
        extension.recognition = _recognition(final_arrives=False)
        extension._has_pending_speech = True

        await extension.finalize("s")
        await extension.finalize("s")

        assert extension.send_asr_finalize_end.await_count == 2

    asyncio.run(_run())


def test_finalize_send_failure_still_ends_once():
    async def _run():
        extension = _extension()
        extension.recognition = _recognition(final_arrives=True)
        extension.recognition.send_silence.side_effect = RuntimeError("closed")
        extension._has_pending_speech = True

        await extension.finalize("s")

        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(_run())
