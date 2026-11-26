#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Results stay with the session whose audio produced them."""

import asyncio

from .mock import (
    FakeClient,
    FakeFrame,
    make_extension,
    outputs,
    patch_client,
    vendor_event,
)

SPEECH = b"\x01\x02" * 1600  # 100 ms at 16 kHz


def _sessions(env) -> list[tuple[str, str]]:
    return [
        (result["text"], result["metadata"]["session_id"])
        for result in outputs(env, "asr_result")
    ]


def test_kept_audio_of_an_older_session_is_not_replayed_for_a_newer_one():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            a_frames = [
                FakeFrame(bytes([1, 2]) * 1600, "s-a") for _ in range(2)
            ]
            b_frames = [
                FakeFrame(bytes([3, 4]) * 1600, "s-b") for _ in range(2)
            ]
            # Disconnected: every frame is kept.
            for frame in a_frames + b_frames:
                await extension._handle_audio_frame(env, frame)

            assert extension.buffered_frames.qsize() == 2
            assert extension.buffered_frames_size == 2 * len(SPEECH)
            assert extension.dropped_audio_frames == 2

            await extension._ensure_connection()
            client = FakeClient.instances[-1]
            assert client.sent == [bytes(frame.buf) for frame in b_frames]

            # A delayed result on the recovered stream belongs to s-b.
            await vendor_event(
                extension,
                {"type": "final", "text": "hai", "start_ms": 0, "end_ms": 200},
            )
            assert _sessions(env) == [("hai", "s-b")]

    asyncio.run(_run())


def test_late_result_keeps_the_session_whose_audio_produced_it():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            await extension._handle_audio_frame(env, FakeFrame(SPEECH, "s-a"))
            await vendor_event(extension, {"type": "partial", "text": "mo"})
            # A new session starts on the same stream before A's final.
            await extension._handle_audio_frame(env, FakeFrame(SPEECH, "s-b"))
            assert extension.metadata["session_id"] == "s-b"

            await vendor_event(
                extension,
                {"type": "final", "text": "một", "start_ms": 0, "end_ms": 100},
            )
            await vendor_event(
                extension,
                {
                    "type": "final",
                    "text": "hai",
                    "start_ms": 100,
                    "end_ms": 200,
                },
            )

            assert _sessions(env) == [
                ("mo", "s-a"),
                ("một", "s-a"),
                ("hai", "s-b"),
            ]
            # Labelling a late result does not change the current session.
            assert extension.metadata["session_id"] == "s-b"

    asyncio.run(_run())


def test_buffered_session_then_live_session_results_are_labelled_by_audio():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            # Outage during session A: its audio is kept.
            await extension._handle_audio_frame(env, FakeFrame(SPEECH, "s-a"))
            await extension._ensure_connection()
            # Recovery drained A; session B then streams live.
            await extension._handle_audio_frame(env, FakeFrame(SPEECH, "s-b"))
            await vendor_event(
                extension,
                {"type": "final", "text": "một", "start_ms": 0, "end_ms": 100},
            )
            await vendor_event(
                extension,
                {
                    "type": "final",
                    "text": "hai",
                    "start_ms": 100,
                    "end_ms": 200,
                },
            )
            assert _sessions(env) == [("một", "s-a"), ("hai", "s-b")]

    asyncio.run(_run())
