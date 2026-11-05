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
