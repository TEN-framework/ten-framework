import asyncio

from iflytek_asr_python.reconnect_manager import ReconnectManager

from .helpers import FakeClient, create_extension, data_as_dict


def _status_payloads(ten_env: object) -> list[dict[str, object]]:
    return [
        data_as_dict(data)
        for data in ten_env.data  # type: ignore[attr-defined]
        if data.get_name() == "connection_status_changed"
    ]


def test_initial_connection_reports_complete_status_payloads() -> None:
    async def run() -> None:
        client = FakeClient()
        client.connected = False
        extension, ten_env, _ = create_extension(client=client)

        await extension.start_connection()

        payloads = _status_payloads(ten_env)
        assert [payload["current"] for payload in payloads] == [
            "connecting",
            "connected",
        ]
        assert [
            (payload["last"], payload["current"]) for payload in payloads
        ] == [
            ("disconnected", "connecting"),
            ("connecting", "connected"),
        ]

        for payload in payloads:
            assert payload["id"]
            assert payload["module"] == "asr"
            assert payload["vendor_info"] == {
                "vendor": "iflytek",
                "code": "",
                "message": "",
            }
            assert isinstance(payload["code"], int)
            assert isinstance(payload["message"], str)
            assert payload["metadata"] == {
                "vendor_metadata": {"language": "en-US"}
            }

    asyncio.run(run())


def test_failed_initial_connection_reports_reconnect_sequence() -> None:
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

        payloads = _status_payloads(ten_env)
        assert [payload["current"] for payload in payloads] == [
            "connecting",
            "disconnected",
            "connecting",
            "connected",
        ]
        assert [
            (payload["last"], payload["current"]) for payload in payloads
        ] == [
            ("disconnected", "connecting"),
            ("connecting", "disconnected"),
            ("disconnected", "connecting"),
            ("connecting", "connected"),
        ]
        assert payloads[1]["code"] == 1000

    asyncio.run(run())


def test_duplicate_disconnected_status_is_suppressed() -> None:
    async def run() -> None:
        client = FakeClient()
        client.connected = False
        extension, ten_env, _ = create_extension(client=client)

        await extension.start_connection()
        await extension.on_closed(terminal_received=False)
        await extension.on_closed(terminal_received=False)

        payloads = _status_payloads(ten_env)
        assert [payload["current"] for payload in payloads] == [
            "connecting",
            "connected",
            "disconnected",
        ]

    asyncio.run(run())
