#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from .mock import (
    FakeClient,
    FakeFrame,
    MockBlazeSTTServer,
    make_extension,
    outputs,
    patch_client,
    wait_until,
)


def test_no_reconnect_after_stop():
    async def _run():
        with patch_client():
            extension, _ = make_extension()
            await extension._ensure_connection()
            client = FakeClient.instances[0]

            extension.stopped = True
            await extension.stop_connection()
            await client.server_close(1006)
            await asyncio.sleep(0.05)

            assert client.closed
            assert len(FakeClient.instances) == 1
            assert extension._reconnect_task is None

    asyncio.run(_run())


def test_stop_cancels_a_pending_reconnect():
    async def _run():
        with patch_client(connect_errors=[OSError("down")] * 100):
            extension, _ = make_extension()
            await extension._ensure_connection()
            await wait_until(lambda: extension._reconnect_task is not None)

            extension.stopped = True
            await extension.stop_connection()
            attempts = len(FakeClient.instances)
            await asyncio.sleep(0.05)

            assert extension._reconnect_task is None
            assert len(FakeClient.instances) == attempts

    asyncio.run(_run())


def test_stale_close_from_a_replaced_socket_is_ignored():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            old = FakeClient.instances[0]

            # Replace the connection, then let the old socket report close.
            await extension._close_client()
            await extension._ensure_connection()
            await old.server_close(1006)
            await asyncio.sleep(0.05)

            assert extension.is_connected()
            assert extension.client is FakeClient.instances[1]
            assert outputs(env, "error") == []

    asyncio.run(_run())


def test_kept_audio_is_drained_when_ready_without_a_new_frame():
    async def _run():
        with patch_client():
            extension, _ = make_extension()
            kept = [FakeFrame(b"\x01\x00" * 160, "s-old")] * 3
            for frame in kept:
                extension.buffered_frames.put_nowait(frame)
                extension.buffered_frames_size += len(frame.buf)

            await extension._ensure_connection()

            client = FakeClient.instances[0]
            assert client.sent == [bytes(frame.buf) for frame in kept]
            assert extension.buffered_frames.empty()
            assert extension.buffered_frames_size == 0
            # Each drained frame carries its own session.
            assert extension.session_id == "s-old"

    asyncio.run(_run())


def test_real_socket_close_recovers_on_a_new_socket():
    """The vendor drops the socket mid-stream; the receive loop that sees
    the close must not cancel the recovery it triggers."""

    async def drop_after_first_audio(ws, server):
        await ws.recv()
        await ws.close(1011, "internal error")
        del server

    async def _run():
        async with MockBlazeSTTServer(script=drop_after_first_audio) as server:
            extension, _ = make_extension(base_url=server.base_url)
            from unittest.mock import patch

            from .. import extension as extension_module

            with patch.object(
                extension_module, "RECONNECT_BASE_DELAY_SECONDS", 0
            ):
                await extension._ensure_connection()
                first = extension.client
                await extension.send_audio(FakeFrame(b"\x00\x01" * 160), "s1")

                await wait_until(
                    lambda: server.connections == 2
                    and extension.is_connected(),
                    timeout=5.0,
                )
                assert extension.client is not first
                extension.stopped = True
                await extension.stop_connection()

    asyncio.run(_run())
