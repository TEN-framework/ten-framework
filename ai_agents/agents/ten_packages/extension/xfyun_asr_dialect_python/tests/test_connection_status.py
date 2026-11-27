import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

from .. import recognition
from ..extension import XfyunDialectASRExtension
from .test_reconnect_audio import ProviderSocket


def make_environment():
    config = {
        "params": {
            "app_id": "test-app",
            "access_key_id": "test-key",
            "access_key_secret": "test-secret",
        }
    }
    env = MagicMock()
    env.get_property_bool = AsyncMock(return_value=(True, None))
    env.get_property_to_json = AsyncMock(
        return_value=(json.dumps(config), None)
    )
    env.send_data = AsyncMock()
    return env


def emitted_payloads(env, name):
    return [
        json.loads(call.args[0].get_property_to_json(None)[0])
        for call in env.send_data.await_args_list
        if call.args[0].get_name() == name
    ]


async def wait_for_connected(env, count=1):
    async def wait():
        while (
            sum(
                event["current"] == "connected"
                for event in emitted_payloads(env, "connection_status_changed")
            )
            < count
        ):
            await asyncio.sleep(0)

    await asyncio.wait_for(wait(), timeout=1)


def test_connection_reports_ready_and_stopped_states():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        socket = ProviderSocket()
        connect = AsyncMock(return_value=socket)
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

        assert [
            event["current"]
            for event in emitted_payloads(env, "connection_status_changed")
        ] == ["connecting", "connected", "disconnected"]
        assert connect.await_count == 1
        assert emitted_payloads(env, "error") == []

    asyncio.run(scenario())


def test_handshake_failure_reports_disconnect_and_recovers():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        socket = ProviderSocket()
        connect = AsyncMock(
            side_effect=[OSError("network unavailable"), socket]
        )
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 0
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
                assert [
                    event["current"]
                    for event in emitted_payloads(
                        env, "connection_status_changed"
                    )
                ] == ["connecting", "disconnected", "connecting", "connected"]
                assert connect.await_count == 2
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_stop_cancels_pending_reconnect():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        connect = AsyncMock(side_effect=OSError("network unavailable"))
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 60
            await extension.on_start(env)
            retry = extension._reconnect_task
            assert retry is not None
            await asyncio.wait_for(extension.on_stop(env), timeout=1)
            await extension.on_deinit(env)
            await asyncio.sleep(0)

        assert retry.cancelled()
        assert connect.await_count == 1
        assert [
            event["current"]
            for event in emitted_payloads(env, "connection_status_changed")
        ] == ["connecting", "disconnected"]

    asyncio.run(scenario())


def test_old_callbacks_cannot_change_replacement_connection():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        connect = AsyncMock(side_effect=[ProviderSocket(), ProviderSocket()])
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
                old_callback = extension.recognition.callback
                await extension.start_connection()
                await wait_for_connected(env, count=2)

                await old_callback.on_open()
                await old_callback.on_error("old connection failed")
                await old_callback.on_close()
                await asyncio.sleep(0)

                assert [
                    event["current"]
                    for event in emitted_payloads(
                        env, "connection_status_changed"
                    )
                ] == ["connecting", "connected", "connecting", "connected"]
                assert emitted_payloads(env, "error") == []
                assert connect.await_count == 2
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_socket_close_reports_disconnect_before_reconnect():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        first = ProviderSocket()
        connect = AsyncMock(side_effect=[first, ProviderSocket()])
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 0
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
                await first.close()
                await wait_for_connected(env, count=2)

                assert [
                    event["current"]
                    for event in emitted_payloads(
                        env, "connection_status_changed"
                    )
                ] == [
                    "connecting",
                    "connected",
                    "disconnected",
                    "connecting",
                    "connected",
                ]
                assert connect.await_count == 2
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_live_message_error_keeps_connection_open():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        connect = AsyncMock(return_value=ProviderSocket())
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
                await extension.recognition.callback.on_error(
                    "Error processing message"
                )
                assert len(emitted_payloads(env, "error")) == 1
                assert [
                    event["current"]
                    for event in emitted_payloads(
                        env, "connection_status_changed"
                    )
                ] == ["connecting", "connected"]
                assert connect.await_count == 1
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_transient_failures_keep_retrying_until_recovered():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        connect = AsyncMock(
            side_effect=[OSError("network unavailable") for _ in range(6)]
            + [ProviderSocket()]
        )
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 0
            try:
                await extension.on_start(env)
                await wait_for_connected(env)
                assert connect.await_count == 7
                assert all(
                    event["code"] == 1000
                    for event in emitted_payloads(env, "error")
                )
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_explicit_retry_limit_stops_with_fatal_error():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        connect = AsyncMock(side_effect=OSError("network unavailable"))
        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 0
            extension.reconnect_manager.max_attempts = 2
            try:
                await extension.on_start(env)
                await asyncio.wait_for(extension._reconnect_task, timeout=1)
                assert connect.await_count == 3
                errors = emitted_payloads(env, "error")
                assert errors[-1]["code"] == -1000
                assert sum(error["code"] == -1000 for error in errors) == 1
            finally:
                await extension.on_stop(env)
                await extension.on_deinit(env)

    asyncio.run(scenario())


def test_stop_cancels_reconnect_during_handshake():
    async def scenario():
        env = make_environment()
        extension = XfyunDialectASRExtension("asr")
        reconnect_started = asyncio.Event()
        attempts = 0

        async def connect(*_args, **_kwargs):
            nonlocal attempts
            attempts += 1
            if attempts == 1:
                raise OSError("network unavailable")
            reconnect_started.set()
            await asyncio.Event().wait()

        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(env)
            extension.reconnect_manager.base_delay = 0
            await extension.on_start(env)
            await asyncio.wait_for(reconnect_started.wait(), timeout=1)
            await asyncio.wait_for(extension.on_stop(env), timeout=1)
            await extension.on_deinit(env)

        assert attempts == 2
        assert [
            event["current"]
            for event in emitted_payloads(env, "connection_status_changed")
        ] == ["connecting", "disconnected", "connecting", "disconnected"]

    asyncio.run(scenario())
