#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from .mock import make_extension, outputs, vendor_event


class FakeTimeline:
    def get_audio_duration_before_time(self, value: int) -> int:
        return value


def _extension(**params):
    extension, env = make_extension(**params)
    extension.audio_timeline = FakeTimeline()
    return extension, env


def test_final_result_emitted_json_shape():
    async def _run():
        extension, env = _extension()
        extension.metadata = {"session_id": "session-123", "turn_id": 4}

        await vendor_event(
            extension,
            {
                "type": "final",
                "text": " xin chào.",
                "start_ms": 120,
                "end_ms": 460,
            },
        )

        (result,) = outputs(env, "asr_result")
        assert result["text"] == "xin chào."
        assert result["final"] is True
        assert result["start_ms"] == 120
        assert result["duration_ms"] == 340
        assert result["language"] == "vi-VN"
        assert result["id"]
        assert result["metadata"]["session_id"] == "session-123"
        assert "session_id" not in result["metadata"]["asr_info"]
        assert result["metadata"]["asr_info"]["vendor"] == "blaze"
        assert result["metadata"]["asr_info"]["turn_id"] == 4

    asyncio.run(_run())


def test_partial_is_interim_starts_after_last_final_and_shares_turn_id():
    async def _run():
        extension, env = _extension()
        await vendor_event(
            extension,
            {"type": "final", "text": "một.", "start_ms": 0, "end_ms": 700},
        )
        await vendor_event(extension, {"type": "partial", "text": "hai"})
        await vendor_event(extension, {"type": "partial", "text": "hai ba"})

        first_final, partial, partial2 = outputs(env, "asr_result")
        assert partial["final"] is False
        assert partial["start_ms"] == 700
        assert partial["duration_ms"] == 0
        assert partial["metadata"]["asr_info"]["locked"] is False
        # One base-generated id per turn; a final starts a new one.
        assert partial["id"] == partial2["id"] != first_final["id"]

    asyncio.run(_run())


def test_empty_results_are_not_emitted():
    async def _run():
        extension, env = _extension()
        await vendor_event(extension, {"type": "partial", "text": "  "})
        await vendor_event(
            extension, {"type": "final", "text": "", "start_ms": 0}
        )
        assert outputs(env, "asr_result") == []

    asyncio.run(_run())


def test_metadata_is_not_mutated():
    async def _run():
        extension, _ = _extension()
        extension.metadata = {"session_id": "session-123"}
        await vendor_event(
            extension, {"type": "final", "text": "hello", "start_ms": 0}
        )
        assert extension.metadata == {"session_id": "session-123"}

    asyncio.run(_run())


def test_offsets_continue_after_reconnect():
    async def _run():
        extension, env = _extension()
        extension.sent_user_audio_duration_ms_before_last_reset = 5000

        await vendor_event(
            extension,
            {"type": "final", "text": "hello", "start_ms": 200, "end_ms": 400},
        )

        (result,) = outputs(env, "asr_result")
        assert result["start_ms"] == 5200
        assert result["duration_ms"] == 200

    asyncio.run(_run())


def test_result_language_override():
    async def _run():
        extension, env = _extension(result_language_default="vi")
        await vendor_event(
            extension, {"type": "final", "text": "hello", "start_ms": 0}
        )
        assert outputs(env, "asr_result")[0]["language"] == "vi"

    asyncio.run(_run())


def test_results_from_a_replaced_socket_are_ignored():
    async def _run():
        extension, env = _extension()
        stale = extension._generation
        extension._generation += 1
        await extension._handle_message(
            {"type": "final", "text": "late", "start_ms": 0}, generation=stale
        )
        assert outputs(env, "asr_result") == []

    asyncio.run(_run())
