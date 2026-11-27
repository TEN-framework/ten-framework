import asyncio
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest
from qwen_asr_python.client import QwenASRClient, QwenProtocolError
from websockets.protocol import State
from websockets.datastructures import Headers


class FakeWebSocket:
    def __init__(self, incoming):
        self.incoming = asyncio.Queue()
        for item in incoming:
            self.incoming.put_nowait(item)
        self.sent = []
        self.state = State.OPEN

    async def send(self, payload):
        self.sent.append(payload)
        if isinstance(payload, str) and json.loads(payload).get("finish"):
            await self.incoming.put(
                json.dumps(
                    {"header": {"event": "task-finished"}, "payload": {}}
                )
            )

    async def recv(self):
        return await self.incoming.get()

    async def close(self):
        self.state = State.CLOSED


def test_client_waits_for_task_started_before_audio(monkeypatch):
    async def run():
        fake = FakeWebSocket(
            [
                json.dumps(
                    {"header": {"event": "task-started"}, "payload": {}}
                ),
            ]
        )
        callbacks = []
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {"run": task_id},
            lambda task_id: {"finish": task_id},
            lambda event: _append(callbacks, event),
            lambda reason, exc: _append(
                callbacks, {"close": reason, "exc": exc}
            ),
            timeout_s=1,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect",
            AsyncMock(return_value=fake),
        )
        await client.connect()
        assert client.task_ready
        await client.send_audio(b"pcm")
        await client.finish_task()
        assert json.loads(fake.sent[0]) == {"run": client.task_id}
        assert fake.sent[1] == b"pcm"
        assert json.loads(fake.sent[2]) == {"finish": client.task_id}
        assert not client.task_ready
        await client.close()

    asyncio.run(run())


async def _append(items, value):
    items.append(value)


def test_client_ignores_stale_task_events():
    async def run():
        callbacks = []
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {},
            lambda task_id: {},
            lambda event: _append(callbacks, event),
            lambda reason, exc: asyncio.sleep(0),
            timeout_s=1,
        )
        client.task_id = "task-b"
        client.task_uuid = "vendor-task-b"
        client.request_id = "request-b"

        await client._handle_event(
            {
                "header": {
                    "event": "task-failed",
                    "task_id": "task-a",
                    "task_uuid": "vendor-task-a",
                },
                "payload": {
                    "error": {"code": "old", "message": "stale"},
                    "output": {"request_id": "request-a"},
                },
            }
        )

        assert callbacks == []
        assert not client._task_failed.is_set()
        assert client.task_uuid == "vendor-task-b"
        assert client.request_id == "request-b"

    asyncio.run(run())


def test_client_extracts_connection_and_task_ids_and_resets_next_task(
    monkeypatch,
):
    async def run():
        fake = FakeWebSocket([])
        fake.response = SimpleNamespace(
            headers=Headers({"X-Request-Id": "connection-request"})
        )
        started_tasks = []

        async def send(payload):
            message = json.loads(payload)
            started_tasks.append(message["run"])
            await fake.incoming.put(
                json.dumps(
                    {
                        "header": {
                            "event": "task-started",
                            "task_id": message["run"],
                            "task_uuid": f"vendor-task-{len(started_tasks)}",
                        },
                        "payload": {},
                    }
                )
            )

        fake.send = AsyncMock(side_effect=send)
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {"run": task_id},
            lambda task_id: {"finish": task_id},
            lambda event: asyncio.sleep(0),
            lambda reason, exc: asyncio.sleep(0),
            timeout_s=1,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect",
            AsyncMock(return_value=fake),
        )
        await client.connect()
        assert client.connection_request_id == "connection-request"
        assert client.task_uuid == "vendor-task-1"
        await client._handle_event(
            {
                "header": {
                    "event": "result-generated",
                    "task_id": client.task_id,
                },
                "payload": {
                    "request_id": "legacy-request",
                    "output": {"request_id": "result-request"},
                },
            }
        )
        assert client.request_id == "result-request"

        await client.start_task()

        assert started_tasks[0] != started_tasks[1]
        assert client.task_uuid == "vendor-task-2"
        assert client.request_id is None
        assert client.connection_request_id == "connection-request"
        await client.close()

    asyncio.run(run())


def test_finish_task_clears_ready_on_timeout(monkeypatch):
    async def run():
        fake = FakeWebSocket(
            [
                json.dumps(
                    {"header": {"event": "task-started"}, "payload": {}}
                ),
            ]
        )
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {"run": task_id},
            lambda task_id: {"finish": task_id},
            lambda event: asyncio.sleep(0),
            lambda reason, exc: asyncio.sleep(0),
            timeout_s=0.05,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect",
            AsyncMock(return_value=fake),
        )
        await client.connect()
        assert client.task_ready
        # Override send so finish-task never yields task-finished.
        fake.send = AsyncMock()
        with pytest.raises(QwenProtocolError) as exc_info:
            await client.finish_task()
        assert exc_info.value.code == "timeout"
        assert not client.task_ready
        await client.close()

    asyncio.run(run())


def test_client_reports_task_failure(monkeypatch):
    async def run():
        fake = FakeWebSocket(
            [
                json.dumps(
                    {
                        "header": {"event": "task-failed"},
                        "payload": {"error": {"code": "bad", "message": "no"}},
                    }
                )
            ]
        )
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {},
            lambda task_id: {},
            lambda event: asyncio.sleep(0),
            lambda reason, exc: asyncio.sleep(0),
            timeout_s=1,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect",
            AsyncMock(return_value=fake),
        )
        try:
            await client.connect()
        except QwenProtocolError as exc:
            assert exc.code == "bad"
        else:
            raise AssertionError("expected task failure")

    asyncio.run(run())
