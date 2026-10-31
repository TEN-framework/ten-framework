import asyncio
import json
from unittest.mock import AsyncMock, MagicMock

import pytest
from websockets.datastructures import Headers
from websockets.exceptions import InvalidStatus
from websockets.http11 import Response

from ten_ai_base.message import ModuleErrorCode

from qwen_asr_python.config import QwenASRConfig
from qwen_asr_python.extension import QwenASRExtension


class FakeTimeline:
    total_user_audio_duration = 0
    total_silence_audio_duration = 0

    def get_audio_duration_before_time(self, value):
        return value

    def reset(self):
        pass


def configured_extension():
    extension = QwenASRExtension("qwen_asr_python")
    extension.ten_env = MagicMock()
    extension.audio_timeline = FakeTimeline()
    extension.config = QwenASRConfig(
        params={"api_key": "secret", "language_hints": ["zh"]}
    )
    extension.config.apply_defaults()
    extension.config.validate_config()
    extension.metadata = {"session_id": "session-1", "turn_id": 7}
    extension.send_asr_result = AsyncMock()
    return extension


def test_result_maps_sentence_words_and_metadata():
    async def run():
        extension = configured_extension()
        await extension._handle_result(
            {
                "payload": {
                    "output": {
                        "sentence": {
                            "begin_time": 160,
                            "end_time": 1640,
                            "text": "欢迎使用阿里云。",
                            "sentence_end": True,
                            "sentence_id": 1,
                            "words": [
                                {
                                    "begin_time": 160,
                                    "end_time": 520,
                                    "text": "欢迎",
                                    "fixed": True,
                                },
                                {
                                    "begin_time": 520,
                                    "end_time": 880,
                                    "text": "使用",
                                    "fixed": True,
                                },
                            ],
                        }
                    },
                    "request_id": "request-1",
                }
            }
        )
        result = extension.send_asr_result.await_args.args[0]
        assert result.text == "欢迎使用阿里云。"
        assert result.final is True
        assert result.start_ms == 160
        assert result.duration_ms == 1480
        assert result.language == "zh-CN"
        assert result.metadata["session_id"] == "session-1"
        assert result.metadata["asr_info"]["request_id"] == "request-1"
        assert result.words[0].stable is True

    asyncio.run(run())


def test_empty_and_heartbeat_results_are_ignored():
    async def run():
        extension = configured_extension()
        await extension._handle_result({"payload": {"heartbeat": True}})
        await extension._handle_result(
            {"payload": {"output": {"sentence": {"text": ""}}}}
        )
        extension.send_asr_result.assert_not_awaited()

    asyncio.run(run())


def connection_extension():
    extension = configured_extension()
    extension.ten_env.send_data = AsyncMock()
    return extension


def fake_client(error=None):
    client = MagicMock()
    client.connected = error is None
    client.connect = AsyncMock(side_effect=error)
    client.close = AsyncMock()
    return client


def emitted_payloads(extension, name):
    return [
        json.loads(call.args[0].get_property_to_json(None)[0])
        for call in extension.ten_env.send_data.await_args_list
        if call.args[0].get_name() == name
    ]


@pytest.mark.parametrize("status", [401, 403])
def test_auth_failure_disconnects_without_retry(monkeypatch, status):
    async def run():
        extension = connection_extension()
        failure = InvalidStatus(Response(status, "Unauthorized", Headers()))
        client = fake_client(failure)
        factory = MagicMock(return_value=client)
        monkeypatch.setattr("qwen_asr_python.extension.QwenASRClient", factory)

        await extension.start_connection()

        assert [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ] == ["connecting", "disconnected"]
        errors = emitted_payloads(extension, "error")
        assert len(errors) == 1
        assert errors[0]["code"] == int(ModuleErrorCode.FATAL_ERROR.value)
        assert errors[0]["vendor_info"]["vendor"] == "qwen"
        assert extension._fatal_error
        assert extension._reconnect_task is None
        assert extension.client is None
        assert not extension.is_connected()
        client.close.assert_awaited_once()
        extension._schedule_reconnect()
        assert extension._reconnect_task is None
        factory.assert_called_once()

    asyncio.run(run())


def test_transient_failure_retries_and_recovers_once(monkeypatch):
    async def run():
        extension = connection_extension()
        failed = fake_client(OSError("connection reset"))
        recovered = fake_client()
        factory = MagicMock(side_effect=[failed, recovered])
        monkeypatch.setattr("qwen_asr_python.extension.QwenASRClient", factory)

        await extension.start_connection()
        reconnect_task = extension._reconnect_task
        assert reconnect_task is not None
        extension._schedule_reconnect()
        assert extension._reconnect_task is reconnect_task
        await asyncio.wait_for(reconnect_task, timeout=5)

        assert [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ] == ["connecting", "disconnected", "connecting", "connected"]
        errors = emitted_payloads(extension, "error")
        assert len(errors) == 1
        assert errors[0]["code"] == int(ModuleErrorCode.NON_FATAL_ERROR.value)
        assert extension.client is recovered
        assert extension.is_connected()
        assert factory.call_count == 2
        assert extension._reconnect_task is None
        failed.close.assert_awaited_once()

    asyncio.run(run())


def test_reconnect_attempts_are_bounded(monkeypatch):
    async def run():
        extension = connection_extension()
        extension.config.reconnect_max_attempts = 2
        client = fake_client(OSError("network unavailable"))
        factory = MagicMock(return_value=client)
        monkeypatch.setattr("qwen_asr_python.extension.QwenASRClient", factory)

        await extension.start_connection()
        await asyncio.wait_for(extension._reconnect_task, timeout=5)

        assert factory.call_count == 3
        assert client.close.await_count == 3
        assert [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ] == ["connecting", "disconnected"] * 3
        assert extension._reconnect_task is None
        assert not extension.is_connected()

    asyncio.run(run())


def test_stop_cancels_pending_reconnect(monkeypatch):
    async def run():
        extension = connection_extension()
        client = fake_client(OSError("network unavailable"))
        factory = MagicMock(return_value=client)
        monkeypatch.setattr("qwen_asr_python.extension.QwenASRClient", factory)

        await extension.start_connection()
        reconnect_task = extension._reconnect_task
        assert reconnect_task is not None
        await asyncio.sleep(0)
        await extension.on_stop(extension.ten_env)

        assert reconnect_task.cancelled()
        assert extension._reconnect_task is None
        assert extension.stopped
        extension._schedule_reconnect()
        assert extension._reconnect_task is None
        factory.assert_called_once()

    asyncio.run(run())
