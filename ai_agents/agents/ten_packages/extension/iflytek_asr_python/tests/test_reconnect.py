import asyncio

from iflytek_asr_python.reconnect_manager import ReconnectManager

from .helpers import FakeClient, create_extension, data_as_dict


def test_disconnect_automatically_reconnects_and_resets_counter() -> None:
    async def run() -> None:
        client = FakeClient(start_errors=[ConnectionError("offline")])
        client.connected = False
        extension, ten_env, _ = create_extension(client=client)
        extension._should_reconnect = True

        async def no_delay(_delay: float) -> None:
            return None

        extension.reconnect_manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=3,
            sleep=no_delay,
        )

        await extension.on_closed(terminal_received=False)
        assert extension._reconnect_task is not None
        await extension._reconnect_task

        errors = [
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "error"
        ]
        assert [error["code"] for error in errors] == [1000]
        assert client.start_calls == 2
        assert client.connected is True
        assert extension.reconnect_manager.current_attempts == 0

    asyncio.run(run())


def test_initial_network_failure_is_nonfatal_and_retries() -> None:
    async def run() -> None:
        client = FakeClient(start_errors=[ConnectionError("offline")])
        client.connected = False
        extension, ten_env, _ = create_extension(client=client)

        async def no_delay(_delay: float) -> None:
            return None

        extension.reconnect_manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=2,
            sleep=no_delay,
        )

        await extension.start_connection()
        reconnect_task = extension._reconnect_task
        assert reconnect_task is not None
        await reconnect_task

        errors = [
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "error"
        ]
        assert [error["code"] for error in errors] == [1000]
        assert client.start_calls == 2
        assert client.connected is True

    asyncio.run(run())
