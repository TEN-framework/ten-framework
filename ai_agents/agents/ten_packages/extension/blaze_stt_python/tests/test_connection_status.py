#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from websockets.exceptions import ConnectionClosedOK
from websockets.frames import Close

from ..recognition import BlazeSTTAuthError
from .mock import (
    FakeClient,
    connection_states,
    make_extension,
    outputs,
    patch_client,
    wait_until,
)


def test_handshake_reports_connecting_then_connected():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()

            assert connection_states(env) == ["connecting", "connected"]
            assert extension.is_connected()

    asyncio.run(_run())


def test_connect_failure_closes_connecting_with_disconnected():
    async def _run():
        with patch_client(connect_errors=[BlazeSTTAuthError("bad token")]):
            extension, env = make_extension()
            await extension._ensure_connection()

            assert connection_states(env) == ["connecting", "disconnected"]
            assert not extension.is_connected()

    asyncio.run(_run())


def test_socket_close_reports_disconnected_then_recovers():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            await FakeClient.instances[0].server_close(1006)
            await wait_until(extension.is_connected)

            assert connection_states(env) == [
                "connecting",
                "connected",
                "disconnected",
                "connecting",
                "connected",
            ]

    asyncio.run(_run())


def test_normal_close_reports_disconnected_without_an_error():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            await FakeClient.instances[0].server_close(
                1000,
                ConnectionClosedOK(Close(1000, ""), Close(1000, ""), True),
            )
            await wait_until(extension.is_connected)

            assert outputs(env, "error") == []
            assert "disconnected" in connection_states(env)

    asyncio.run(_run())


def test_status_payload_redacts_the_api_key():
    async def _run():
        with patch_client():
            extension, env = make_extension(api_key="super-secret-key")
            await extension._ensure_connection()

            event = outputs(env, "connection_status_changed")[-1]
            assert event["vendor_info"]["vendor"] == "blaze"
            vendor_metadata = event["metadata"]["vendor_metadata"]
            assert vendor_metadata["model"] == "stt-stream-1.5"
            assert "super-secret-key" not in str(event)

    asyncio.run(_run())
