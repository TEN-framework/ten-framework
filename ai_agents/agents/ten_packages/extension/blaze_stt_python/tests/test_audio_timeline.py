#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from .mock import (
    FakeClient,
    FakeFrame,
    finalize_data,
    make_extension,
    outputs,
    patch_client,
    vendor_event,
    wait_until,
)

MS_100 = b"\x01\x02" * 1600  # 100 ms of speech-level audio at 16 kHz


def test_only_audio_blaze_received_is_counted():
    async def _run():
        with patch_client():
            extension, _ = make_extension()
            await extension._ensure_connection()
            client = FakeClient.instances[0]

            assert await extension.send_audio(FakeFrame(MS_100), "s1")
            client.send_error = ConnectionError("broken pipe")
            assert not await extension.send_audio(FakeFrame(MS_100), "s1")

            assert (
                extension.audio_timeline.get_total_user_audio_duration() == 100
            )

    asyncio.run(_run())


def test_finalize_silence_is_silence_not_user_audio():
    async def _run():
        with patch_client():
            extension, env = make_extension(finalize_silence_ms=200)
            await extension._ensure_connection()
            await extension.send_audio(FakeFrame(MS_100), "s1")
            await extension.on_data(env, finalize_data("f-1", "s1"))

            timeline = extension.audio_timeline
            assert timeline.get_total_user_audio_duration() == 100
            # Vendor time 300 ms = 100 ms speech + 200 ms injected silence.
            assert timeline.get_audio_duration_before_time(300) == 100

    asyncio.run(_run())


def test_timestamps_continue_across_connections():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            await extension.send_audio(FakeFrame(MS_100 * 5), "s1")

            await FakeClient.instances[0].server_close(1006)
            await wait_until(extension.is_connected)
            await extension.send_audio(FakeFrame(MS_100 * 3), "s1")
            # Blaze restarts its clock on the new socket.
            await vendor_event(
                extension,
                {"type": "final", "text": "hai", "start_ms": 0, "end_ms": 300},
            )

            (result,) = outputs(env, "asr_result")
            assert result["start_ms"] == 500
            assert result["duration_ms"] == 300

    asyncio.run(_run())
