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
)

SPEECH = b"\x01\x02" * 1600  # 100 ms at 16 kHz


async def _connected():
    extension, env = make_extension()
    await extension._ensure_connection()
    assert extension.is_connected()
    return extension, env, FakeClient.instances[-1]


def _silence_sent(client: FakeClient) -> bool:
    return any(chunk and not any(chunk) for chunk in client.sent)


def test_disconnected_finalize_echoes_request_and_ends_once():
    async def _run():
        extension, env = make_extension()
        await extension.on_data(env, finalize_data("f-1", "s-req"))

        (end,) = outputs(env, "asr_finalize_end")
        assert end == {
            "finalize_id": "f-1",
            "metadata": {"session_id": "s-req"},
        }

    asyncio.run(_run())


def test_no_audio_since_last_final_ends_without_silence():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            await extension.on_data(env, finalize_data("f-1", "s1"))

            assert not _silence_sent(client)
            assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


def test_audio_without_any_partial_is_still_finalized():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            assert await extension.send_audio(FakeFrame(SPEECH), "s1")

            async def final_arrives():
                await asyncio.sleep(0.05)
                await vendor_event(
                    extension,
                    {"type": "final", "text": "xin chào", "start_ms": 0},
                )

            asyncio.create_task(final_arrives())
            await extension.on_data(env, finalize_data("f-1", "s1"))

            assert _silence_sent(client)
            names = [
                call.args[0].get_name() for call in env.send_data.call_args_list
            ]
            # The final result goes out before the finalize end.
            assert names.index("asr_result") < names.index("asr_finalize_end")
            assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


def test_timeout_without_final_still_ends_once():
    async def _run():
        with patch_client():
            extension, env, _ = await _connected()
            await extension.send_audio(FakeFrame(SPEECH), "s1")
            await extension.on_data(env, finalize_data("f-1", "s1"))
            assert outputs(env, "asr_finalize_end") == [
                {"finalize_id": "f-1", "metadata": {"session_id": "s1"}}
            ]

    asyncio.run(_run())


def test_silence_send_failure_still_ends_once():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            await extension.send_audio(FakeFrame(SPEECH), "s1")
            client.send_error = ConnectionError("socket gone")
            await extension.on_data(env, finalize_data("f-1", "s1"))
            assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


def test_concurrent_finalizes_echo_independent_ids_and_sessions():
    async def _run():
        with patch_client():
            extension, env, _ = await _connected()
            await extension.send_audio(FakeFrame(SPEECH, "s-a"), "s-a")

            first = asyncio.create_task(
                extension.on_data(env, finalize_data("f-a", "s-a"))
            )
            await asyncio.sleep(0)
            # A later frame and a second finalize while the first waits.
            extension.metadata = {"session_id": "s-b"}
            second = asyncio.create_task(
                extension.on_data(env, finalize_data("f-b", "s-b"))
            )
            await asyncio.gather(first, second)

            assert outputs(env, "asr_finalize_end") == [
                {"finalize_id": "f-a", "metadata": {"session_id": "s-a"}},
                {"finalize_id": "f-b", "metadata": {"session_id": "s-b"}},
            ]

    asyncio.run(_run())


def test_disconnected_finalize_discards_kept_audio():
    async def _run():
        extension, env = make_extension()
        extension.buffered_frames.put_nowait(FakeFrame(SPEECH))
        extension.buffered_frames_size = len(SPEECH)

        await extension.on_data(env, finalize_data("f-1", "s1"))

        assert extension.buffered_frames.empty()
        assert extension.buffered_frames_size == 0
        assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


QUIET = b"\x01\x00" * 1600  # 100 ms of background-level audio at 16 kHz


def test_late_final_for_an_earlier_utterance_does_not_ack_newer_speech():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            # Utterance A (0-100 ms), a pause, then utterance B (800-1100).
            await extension.send_audio(FakeFrame(SPEECH), "s1")
            for _ in range(7):
                await extension.send_audio(FakeFrame(QUIET), "s1")
            for _ in range(3):
                await extension.send_audio(FakeFrame(SPEECH), "s1")
            # A's final arrives only now, after B was already sent.
            await vendor_event(
                extension,
                {"type": "final", "text": "một", "start_ms": 0, "end_ms": 100},
            )

            async def finals_during_wait():
                await asyncio.sleep(0.03)
                # A repeated stale final must not release the finalize.
                await vendor_event(
                    extension,
                    {"type": "final", "text": "", "start_ms": 0, "end_ms": 100},
                )
                await asyncio.sleep(0.03)
                await vendor_event(
                    extension,
                    {
                        "type": "final",
                        "text": "hai",
                        "start_ms": 800,
                        "end_ms": 1100,
                    },
                )

            asyncio.create_task(finals_during_wait())
            await extension.on_data(env, finalize_data("f-b", "s1"))

            assert _silence_sent(client)
            names = [
                (
                    call.args[0].get_name(),
                    call.args[0].get_property_to_json("")[0],
                )
                for call in env.send_data.call_args_list
            ]
            end_index = next(
                i
                for i, (name, _) in enumerate(names)
                if name == "asr_finalize_end"
            )
            b_index = next(
                i
                for i, (name, payload) in enumerate(names)
                if name == "asr_result" and '"hai"' in payload
            )
            assert b_index < end_index

    asyncio.run(_run())


def test_trailing_silence_after_a_final_needs_no_wait():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            await extension.send_audio(FakeFrame(SPEECH), "s1")
            await vendor_event(
                extension,
                {"type": "final", "text": "một", "start_ms": 0, "end_ms": 100},
            )
            for _ in range(7):
                await extension.send_audio(FakeFrame(QUIET), "s1")

            await extension.on_data(env, finalize_data("f-1", "s1"))

            assert not _silence_sent(client)
            assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


def test_finalize_and_stop_do_not_wait_for_a_stalled_handshake():
    async def _run():
        from .mock import MockBlazeSTTServer, wait_until

        async with MockBlazeSTTServer(stall_handshake=True) as server:
            extension, env = make_extension(base_url=server.base_url)
            connecting = asyncio.create_task(extension._ensure_connection())
            await wait_until(lambda: extension._opening)

            loop = asyncio.get_running_loop()
            started = loop.time()
            await extension.on_data(env, finalize_data("f-1", "s1"))
            assert loop.time() - started < 0.5
            assert len(outputs(env, "asr_finalize_end")) == 1

            extension.stopped = True
            started = loop.time()
            await extension.stop_connection()
            await asyncio.wait_for(connecting, 1.0)
            assert loop.time() - started < 1.0

    asyncio.run(_run())


def test_idle_stream_keeps_receiving_silence_until_the_final():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            extension.config.finalize_timeout_ms = 2000
            await extension.send_audio(FakeFrame(SPEECH), "s1")

            async def final_after_endpointing():
                # Blaze needs about 600 ms of silence beyond the first 100 ms.
                await asyncio.sleep(0.6)
                await vendor_event(
                    extension,
                    {"type": "final", "text": "xin chào", "end_ms": 100},
                )

            asyncio.create_task(final_after_endpointing())
            loop = asyncio.get_running_loop()
            started = loop.time()
            await extension.on_data(env, finalize_data("f-1", "s1"))

            silence_chunks = [c for c in client.sent if c and not any(c)]
            assert len(silence_chunks) >= 4
            assert loop.time() - started < 1.0
            assert len(outputs(env, "asr_finalize_end")) == 1

    asyncio.run(_run())


def test_flowing_user_audio_gets_no_extra_silence_while_waiting():
    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            extension.config.finalize_timeout_ms = 500
            await extension.send_audio(FakeFrame(SPEECH), "s1")

            async def keep_streaming():
                for _ in range(10):
                    await asyncio.sleep(0.04)
                    await extension.send_audio(FakeFrame(QUIET), "s1")

            streaming = asyncio.create_task(keep_streaming())
            await extension.on_data(env, finalize_data("f-1", "s1"))
            await streaming

            silence_chunks = [c for c in client.sent if c and not any(c)]
            # Only the initial finalize silence; the stream was never idle.
            assert len(silence_chunks) == 1

    asyncio.run(_run())


def test_short_turn_after_a_final_waits_for_its_own_final():
    """Speech sent after a final arrived is new speech, even when it ends
    inside the end-time tolerance of that final."""

    async def _run():
        with patch_client():
            extension, env, client = await _connected()
            await extension.send_audio(FakeFrame(SPEECH), "s1")  # 0-100
            await vendor_event(
                extension,
                {"type": "final", "text": "một", "start_ms": 0, "end_ms": 100},
            )
            # A short second turn (100-200 ms) that Blaze has not shown any
            # partial for yet when the finalize arrives.
            await extension.send_audio(FakeFrame(SPEECH), "s1")

            async def final_arrives():
                await asyncio.sleep(0.05)
                await vendor_event(
                    extension,
                    {
                        "type": "final",
                        "text": "ok",
                        "start_ms": 100,
                        "end_ms": 200,
                    },
                )

            asyncio.create_task(final_arrives())
            await extension.on_data(env, finalize_data("f-2", "s1"))

            assert _silence_sent(client)
            names = [
                (
                    call.args[0].get_name(),
                    call.args[0].get_property_to_json("")[0],
                )
                for call in env.send_data.call_args_list
            ]
            end_index = next(
                i
                for i, (name, _) in enumerate(names)
                if name == "asr_finalize_end"
            )
            ok_index = next(
                i
                for i, (name, payload) in enumerate(names)
                if name == "asr_result" and '"ok"' in payload
            )
            assert ok_index < end_index

    asyncio.run(_run())


def test_stale_final_does_not_cover_speech_sent_after_a_final():
    async def _run():
        with patch_client():
            extension, _, _ = await _connected()
            await extension.send_audio(FakeFrame(SPEECH), "s1")  # 0-100
            final = {"type": "final", "text": "", "start_ms": 0, "end_ms": 100}
            await vendor_event(extension, final)
            await extension.send_audio(FakeFrame(SPEECH), "s1")  # 100-200
            # A repeat of the old final does not cover the new speech.
            await vendor_event(extension, final)
            assert extension._has_pending_speech()
            await vendor_event(
                extension,
                {"type": "final", "text": "", "start_ms": 100, "end_ms": 200},
            )
            assert not extension._has_pending_speech()

    asyncio.run(_run())
