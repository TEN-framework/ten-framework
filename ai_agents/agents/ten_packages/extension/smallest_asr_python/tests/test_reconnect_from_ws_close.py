import asyncio
import json
from types import SimpleNamespace
from unittest.mock import patch

import aiohttp

from ten_packages.extension.smallest_asr_python.config import SmallestASRConfig
from ten_packages.extension.smallest_asr_python.extension import (
    SmallestASRExtension,
)
from ten_packages.extension.smallest_asr_python.reconnect_manager import (
    ReconnectManager,
)


class FakeTenEnv:
    def __init__(self):
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.infos: list[str] = []
        self.debugs: list[str] = []

    def log_error(self, message: str, **_kwargs) -> None:
        self.errors.append(str(message))

    def log_warn(self, message: str, **_kwargs) -> None:
        self.warnings.append(str(message))

    def log_info(self, message: str, **_kwargs) -> None:
        self.infos.append(str(message))

    def log_debug(self, message: str, **_kwargs) -> None:
        self.debugs.append(str(message))


class MockWebSocket:
    def __init__(self):
        self.closed = False
        self.messages: asyncio.Queue[SimpleNamespace] = asyncio.Queue()

    async def close(self) -> bool:
        self.closed = True
        return True

    async def send_str(self, _data: str) -> bool:
        return True

    async def send_bytes(self, _data: bytes) -> bool:
        return True

    def exception(self):
        return None

    def add_message(self, msg_type, data: dict | None = None) -> None:
        self.messages.put_nowait(
            SimpleNamespace(
                type=msg_type,
                data=json.dumps(data or {}),
            )
        )

    def __aiter__(self):
        async def _gen():
            while not self.closed:
                yield await self.messages.get()

        return _gen()


def test_reconnect_from_ws_close_stops_processing_vendor_messages():
    async def run_test() -> None:
        ten_env = FakeTenEnv()
        extension = SmallestASRExtension("smallest_asr_python")
        extension.ten_env = ten_env
        extension.config = SmallestASRConfig(
            params={"api_key": "fake_api_key", "sample_rate": 16000}
        )
        extension.config.update(extension.config.params)
        extension.reconnect_manager = ReconnectManager(
            max_attempts=1,
            base_delay=0,
            logger=ten_env,
        )
        results = []
        ws_connections = [MockWebSocket(), MockWebSocket()]
        reconnect_count = 0

        async def fake_send_asr_error(*_args, **_kwargs) -> None:
            return None

        async def fake_send_asr_result(asr_result) -> None:
            results.append(asr_result)

        class MockSession:
            def __init__(self, *args, **kwargs) -> None:
                self.closed = False

            async def ws_connect(self, *_args, **_kwargs):
                nonlocal reconnect_count
                reconnect_count += 1
                return ws_connections[1]

            async def close(self) -> None:
                self.closed = True

        extension.send_asr_error = fake_send_asr_error
        extension.send_asr_result = fake_send_asr_result
        extension.ws = ws_connections[0]
        extension.connected = True

        ws_connections[0].add_message(aiohttp.WSMsgType.CLOSED)

        with patch(
            "ten_packages.extension.smallest_asr_python.extension.aiohttp.ClientSession",
            MockSession,
        ):
            extension._message_task = asyncio.create_task(
                extension._process_messages()
            )
            original_message_task = extension._message_task
            try:
                await asyncio.wait_for(original_message_task, timeout=1)

                assert any(
                    "Attempting reconnection" in message
                    for message in ten_env.warnings
                ), "WebSocket close should look like it attempted reconnect"

                for _ in range(10):
                    if reconnect_count:
                        break
                    await asyncio.sleep(0.01)

                ws_connections[1].add_message(
                    aiohttp.WSMsgType.TEXT,
                    {
                        "type": "transcription",
                        "transcript": "hello after reconnect",
                        "is_final": True,
                        "language": "en",
                    },
                )
                await asyncio.sleep(0.1)

                assert results, (
                    "ASR logged a reconnect attempt, but the vendor message "
                    "loop stopped and did not process the next transcript"
                )
            finally:
                extension.stopped = True
                await extension.stop_connection()

    asyncio.run(run_test())
