import asyncio
import json
from unittest.mock import AsyncMock, MagicMock

import pytest
from websockets.datastructures import Headers
from websockets.exceptions import InvalidStatus
from websockets.http11 import Response

from ten_ai_base.message import ModuleErrorCode
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_runtime import AudioFrame, Data

from qwen_asr_python.config import QwenASRConfig
from qwen_asr_python.extension import QwenASRExtension
from qwen_asr_python.client import QwenProtocolError


class FakeTimeline:
    total_user_audio_duration = 0
    total_silence_audio_duration = 0
    total_dropped_audio_duration = 0

    def add_user_audio(self, value):
        self.total_user_audio_duration += value

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


def test_emitted_result_preserves_authoritative_vendor_metadata():
    async def run():
        extension = configured_extension()
        extension.ten_env.send_data = AsyncMock()
        extension.metadata = {
            "session_id": "session-1",
            "asr_info": {"vendor": "input", "request_id": "input"},
        }
        extension.send_asr_result = (
            AsyncASRBaseExtension.send_asr_result.__get__(extension)
        )

        await extension._handle_result(
            {
                "payload": {
                    "output": {
                        "sentence": {
                            "text": "hello",
                            "sentence_end": True,
                            "sentence_id": 2,
                        }
                    },
                    "request_id": "vendor-request",
                }
            }
        )

        emitted = [
            call.args[0]
            for call in extension.ten_env.send_data.await_args_list
            if call.args[0].get_name() == "asr_result"
        ][0]
        payload = json.loads(emitted.get_property_to_json(None)[0])
        assert payload["metadata"]["asr_info"] == {
            "vendor": "qwen",
            "model": "qwen-audio-3.1-asr-flash-streaming",
            "request_id": "vendor-request",
            "sentence_id": 2,
            "locked": False,
            "usage": {},
        }

    asyncio.run(run())


def test_concurrent_finalize_requests_keep_distinct_contexts():
    async def run():
        extension = connection_extension()
        client = MagicMock()
        client.connected = True
        client.task_ready = True
        client.task_id = "task-1"
        release_finish = asyncio.Event()

        async def finish_task():
            await release_finish.wait()

        client.finish_task = AsyncMock(side_effect=finish_task)
        extension.client = client
        extension.connected = True
        acknowledgements = []

        async def capture_ack():
            acknowledgements.append(
                (extension.finalize_id, extension.metadata["session_id"])
            )

        extension.send_asr_finalize_end = capture_ack

        def make_finalize(finalize_id, session_id):
            data = Data.create("asr_finalize")
            data.set_property_from_json(
                None,
                json.dumps(
                    {
                        "finalize_id": finalize_id,
                        "metadata": {"session_id": session_id},
                    }
                ),
            )
            return data

        first = asyncio.create_task(
            extension.on_data(
                extension.ten_env, make_finalize("finalize-a", "session-a")
            )
        )
        await asyncio.sleep(0)
        second = asyncio.create_task(
            extension.on_data(
                extension.ten_env, make_finalize("finalize-b", "session-b")
            )
        )
        await asyncio.sleep(0)
        assert client.finish_task.await_count == 1
        release_finish.set()
        await asyncio.gather(first, second)

        assert acknowledgements == [
            ("finalize-a", "session-a"),
            ("finalize-b", "session-b"),
        ]

    asyncio.run(run())


def test_finalize_end_serializes_request_context():
    async def run():
        extension = configured_extension()
        extension.ten_env.send_data = AsyncMock()
        finalize_data = Data.create("asr_finalize")
        finalize_data.set_property_from_json(
            None,
            json.dumps(
                {
                    "finalize_id": "finalize-42",
                    "metadata": {"session_id": "session-final"},
                }
            ),
        )

        await extension.on_data(extension.ten_env, finalize_data)

        sent = extension.ten_env.send_data.await_args.args[0]
        payload = json.loads(sent.get_property_to_json(None)[0])
        assert payload == {
            "finalize_id": "finalize-42",
            "metadata": {"session_id": "session-final"},
        }

    asyncio.run(run())


def test_vendor_metadata_redacts_url_query_and_is_empty_without_config():
    extension = QwenASRExtension("qwen_asr_python")
    assert extension.vendor_metadata() == {}

    extension = configured_extension()
    extension.config.params["url"] = (
        "wss://example.test/asr?api_key=query-secret&region=us"
    )
    metadata = extension._get_masked_vendor_metadata()
    assert "query-secret" not in json.dumps(metadata)
    assert "api_key" in metadata
    assert "region=us" in metadata["url"]


def test_connection_status_serializes_redacted_vendor_metadata():
    async def run():
        extension = connection_extension()
        extension.config.params["url"] = (
            "wss://example.test/asr?api_key=query-secret&region=us"
        )
        await extension.on_connected()

        payload = emitted_payloads(extension, "connection_status_changed")[0]
        vendor_metadata = payload["metadata"]["vendor_metadata"]
        assert vendor_metadata["url"].startswith("wss://example.test/asr?")
        assert "query-secret" not in json.dumps(payload)
        assert "secret" not in vendor_metadata["api_key"]

    asyncio.run(run())


def test_recovery_flushes_each_buffered_frame_with_its_session_metadata(
    monkeypatch,
):
    async def run():
        extension = connection_extension()
        first = AudioFrame.create("pcm_frame")
        first.set_property_from_json(
            "metadata", json.dumps({"session_id": "session-1"})
        )
        first.alloc_buf(320)
        second = AudioFrame.create("pcm_frame")
        second.set_property_from_json(
            "metadata", json.dumps({"session_id": "session-2"})
        )
        second.alloc_buf(320)
        extension.buffered_frames.put_nowait(first)
        extension.buffered_frames.put_nowait(second)
        extension.buffered_frames_size = 640
        extension.connected = False

        client = fake_client()
        client.task_ready = True
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(return_value=client),
        )
        await extension.start_connection()

        assert client.send_audio.await_count == 2
        assert extension.session_id == "session-2"
        assert extension.buffered_frames.empty()
        assert extension.buffered_frames_size == 0

    asyncio.run(run())


def test_protocol_auth_failure_latches_fatal_recovery():
    extension = configured_extension()
    error = QwenProtocolError("bad key", "InvalidApiKey")
    assert extension._is_fatal_exception(error)
    extension._fatal_error = True
    extension._schedule_reconnect()
    assert extension._reconnect_task is None


@pytest.mark.parametrize("code", ["timeout", "not_connected", "task_not_ready"])
def test_transient_protocol_errors_are_not_fatal(code):
    extension = configured_extension()
    assert not extension._is_fatal_exception(
        QwenProtocolError("temporary", code)
    )


def test_protocol_error_wrapper_name_does_not_make_transient_failure_fatal():
    extension = configured_extension()
    assert not extension._is_fatal_exception(
        QwenProtocolError("quota exceeded", "Throttling.RateQuota")
    )


def test_url_authority_credentials_are_rejected():
    extension = configured_extension()
    extension.config.params["url"] = "wss://user:password@example.test/asr"
    with pytest.raises(ValueError, match="without credentials"):
        extension.config.validate_config()


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
    client.task_ready = True
    client.connect = AsyncMock(side_effect=error)
    client.close = AsyncMock()
    client.send_audio = AsyncMock()
    client.start_task = AsyncMock()
    client.finish_task = AsyncMock()
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
        assert errors[0]["vendor_info"]["code"] == str(status)
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


def test_non_fatal_reconnect_keeps_retrying_without_fatal(monkeypatch):
    async def run():
        extension = connection_extension()
        client = fake_client(OSError("network unavailable"))
        factory = MagicMock(return_value=client)
        monkeypatch.setattr("qwen_asr_python.extension.QwenASRClient", factory)

        real_sleep = asyncio.sleep

        async def fast_sleep(_delay):
            await real_sleep(0)

        monkeypatch.setattr(asyncio, "sleep", fast_sleep)

        await extension.start_connection()
        reconnect_task = extension._reconnect_task
        assert reconnect_task is not None

        for _ in range(100):
            await asyncio.sleep(0)
            if factory.call_count >= 3:
                break
        assert factory.call_count >= 3
        assert not extension._fatal_error
        assert reconnect_task is extension._reconnect_task
        assert not reconnect_task.done()
        assert not extension.is_connected()
        errors = emitted_payloads(extension, "error")
        assert errors
        assert all(
            error["code"] == int(ModuleErrorCode.NON_FATAL_ERROR.value)
            for error in errors
        )

        extension._stop_requested = True
        reconnect_task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await reconnect_task

    asyncio.run(run())


def test_connection_closed_ok_skips_asr_error():
    async def run():
        from websockets.frames import Close
        from websockets.exceptions import ConnectionClosedOK

        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        extension.on_disconnected = AsyncMock()
        extension._schedule_reconnect = MagicMock()
        extension._generation = 1
        reason = ConnectionClosedOK(
            Close(1000, "idle"), Close(1000, "idle"), True
        )

        await extension._handle_close(str(reason), 1, reason)

        extension.send_asr_error.assert_not_awaited()
        extension.on_disconnected.assert_awaited_once()
        assert extension.on_disconnected.await_args.kwargs["code"] == 0
        extension._schedule_reconnect.assert_called_once()

    asyncio.run(run())


def test_connection_closed_error_emits_non_fatal():
    async def run():
        from websockets.frames import Close
        from websockets.exceptions import ConnectionClosedError

        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        extension.on_disconnected = AsyncMock()
        extension._schedule_reconnect = MagicMock()
        extension._generation = 1
        reason = ConnectionClosedError(
            Close(1000, "incomplete"), Close(1000, "incomplete"), False
        )

        await extension._handle_close(str(reason), 1, reason)

        extension.send_asr_error.assert_awaited_once()
        error = extension.send_asr_error.await_args.args[0]
        assert error.code == int(ModuleErrorCode.NON_FATAL_ERROR.value)
        vendor = extension.send_asr_error.await_args.args[1]
        assert vendor.code == "websocket_closed"
        extension.on_disconnected.assert_awaited_once()
        assert (
            extension.on_disconnected.await_args.kwargs["code"]
            == ModuleErrorCode.NON_FATAL_ERROR.value
        )
        extension._schedule_reconnect.assert_called_once()

    asyncio.run(run())


def test_mid_stream_task_failed_reports_vendor_info():
    async def run():
        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        client = MagicMock()
        client.task_ready = True
        client.waiting_for_task_event = False
        client.close = AsyncMock()
        extension.client = client
        extension.connected = True
        extension._generation = 1
        extension.on_disconnected = AsyncMock()

        await extension._handle_event(
            {
                "header": {"event": "task-failed"},
                "payload": {
                    "error": {
                        "code": "Throttling.RateQuota",
                        "message": "quota exceeded",
                    }
                },
            },
            1,
        )
        await extension._task_recovery_task

        assert client.task_ready is False
        extension.send_asr_error.assert_awaited_once()
        error = extension.send_asr_error.await_args.args[0]
        vendor = extension.send_asr_error.await_args.args[1]
        assert error.code == int(ModuleErrorCode.NON_FATAL_ERROR.value)
        assert vendor.code == "Throttling.RateQuota"
        assert vendor.message == "quota exceeded"
        assert not extension._fatal_error
        assert extension._reconnect_task is not None
        extension._reconnect_task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await extension._reconnect_task

    asyncio.run(run())


def test_mid_stream_auth_task_failed_is_fatal():
    async def run():
        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        client = MagicMock()
        client.task_ready = True
        client.waiting_for_task_event = False
        client.close = AsyncMock()
        extension.client = client
        extension._generation = 1
        extension.on_disconnected = AsyncMock()

        await extension._handle_event(
            {
                "header": {"event": "task-failed"},
                "payload": {
                    "error": {
                        "code": "InvalidApiKey",
                        "message": "bad key",
                    }
                },
            },
            1,
        )
        await asyncio.sleep(0)
        if extension._task_recovery_task is not None:
            await extension._task_recovery_task

        assert client.task_ready is False
        assert extension._fatal_error
        error = extension.send_asr_error.await_args.args[0]
        assert error.code == int(ModuleErrorCode.FATAL_ERROR.value)
        assert extension._reconnect_task is None

    asyncio.run(run())


def test_task_failed_while_waiting_is_not_double_reported():
    async def run():
        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        client = MagicMock()
        client.task_ready = False
        client.waiting_for_task_event = True
        extension.client = client
        extension._generation = 1

        await extension._handle_event(
            {
                "header": {"event": "task-failed"},
                "payload": {"error": {"code": "bad", "message": "no"}},
            },
            1,
        )

        extension.send_asr_error.assert_not_awaited()

    asyncio.run(run())


def test_waiting_fatal_task_failure_latches_recovery():
    async def run():
        extension = connection_extension()
        extension.send_asr_error = AsyncMock()
        client = MagicMock()
        client.task_ready = False
        client.waiting_for_task_event = True
        extension.client = client
        extension._generation = 1

        await extension._handle_event(
            {
                "header": {"event": "task-failed"},
                "payload": {
                    "error": {"code": "InvalidApiKey", "message": "bad key"}
                },
            },
            1,
        )

        assert extension._fatal_error
        extension.send_asr_error.assert_not_awaited()
        extension._schedule_reconnect()
        assert extension._reconnect_task is None

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
