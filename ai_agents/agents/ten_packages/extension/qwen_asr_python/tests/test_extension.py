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
        language="zh-CN",
        params={"api_key": "secret"},
    )
    extension.config.update(extension.config.params)
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
        assert result.language == "zh-CN"  # fallback when vendor omits language
        assert result.metadata["session_id"] == "session-1"
        assert result.metadata["asr_info"]["request_id"] == "request-1"
        assert result.words[0].stable is True

    asyncio.run(run())


def test_result_language_uses_vendor_language_hints():
    async def run():
        extension = configured_extension()
        await extension._handle_result(
            {
                "payload": {
                    "output": {
                        "language_hints": ["en"],
                        "sentence": {
                            "text": "hello",
                            "sentence_end": True,
                        },
                    },
                }
            }
        )
        result = extension.send_asr_result.await_args.args[0]
        assert result.language == "en-US"

    asyncio.run(run())


@pytest.mark.parametrize("final", [False, True])
@pytest.mark.parametrize("legacy_request_id", [None, "legacy-request"])
def test_emitted_result_extracts_nested_vendor_request_details(
    final, legacy_request_id
):
    async def run():
        extension = connection_extension()
        extension.send_asr_result = (
            AsyncASRBaseExtension.send_asr_result.__get__(extension)
        )
        client = fake_client()
        client.task_id = "task-1"
        client.task_uuid = "vendor-task-1"
        client.connection_request_id = "connection-request"
        extension.client = client
        event = {
            "header": {
                "task_id": "task-1",
                "task_uuid": "vendor-task-1",
                "event": "result-generated",
            },
            "payload": {
                "request_id": legacy_request_id,
                "output": {
                    "request_id": "vendor-request",
                    "sentence": {"text": "hello", "sentence_end": final},
                },
            },
        }

        await extension._handle_event(event, extension._generation)

        info = emitted_payloads(extension, "asr_result")[0]["metadata"][
            "asr_info"
        ]
        assert info["request_id"] == "vendor-request"
        assert info["task_id"] == "task-1"
        assert info["task_uuid"] == "vendor-task-1"
        assert info["connection_request_id"] == "connection-request"

    asyncio.run(run())


@pytest.mark.parametrize("failure_path", ["recovery", "close", "connect"])
def test_error_keeps_request_details_after_client_cleanup(failure_path):
    async def run():
        extension = connection_extension()
        client = fake_client()
        details = {
            "task_id": "task-1",
            "task_uuid": "vendor-task-1",
            "request_id": "vendor-request",
            "connection_request_id": "connection-request",
        }
        for name, value in details.items():
            setattr(client, name, value)
        extension.client = client
        extension.connected = True
        extension._schedule_reconnect = MagicMock()

        async def close():
            for name in details:
                setattr(client, name, None)

        client.close = AsyncMock(side_effect=close)
        error = QwenProtocolError("failed", "test-error")
        match failure_path:
            case "recovery":
                await extension._recover_client(client, error, False)
            case "close":
                await extension._handle_close(
                    "failed", extension._generation, error
                )
            case "connect":
                await extension._abort_connection_attempt(
                    extension._generation, error
                )

        payload = emitted_payloads(extension, "error")[0]
        assert payload["metadata"]["asr_info"] == details
        assert extension.client is None

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


def test_finalize_does_not_hold_send_lock_while_finishing_task():
    async def run():
        extension = connection_extension()
        client = fake_client()
        release_finish = asyncio.Event()
        finish_started = asyncio.Event()

        async def finish_task():
            finish_started.set()
            await release_finish.wait()
            client.task_ready = False

        client.finish_task = AsyncMock(side_effect=finish_task)
        extension.client = client
        extension.connected = True
        extension.finalize_id = "finalize-1"
        extension.metadata = {"session_id": "session-1"}
        extension.send_asr_finalize_end = AsyncMock()

        finalize_task = asyncio.create_task(extension.finalize("session-1"))
        await finish_started.wait()

        await asyncio.wait_for(extension._send_lock.acquire(), timeout=0.1)
        extension._send_lock.release()

        release_finish.set()
        await finalize_task
        if extension._task_start_task is not None:
            await extension._task_start_task

    asyncio.run(run())


def test_send_does_not_hold_send_lock_while_starting_task():
    async def run():
        extension = connection_extension()
        client = fake_client()
        start_started = asyncio.Event()
        release_start = asyncio.Event()

        async def finish_task():
            client.task_ready = False

        async def start_task():
            start_started.set()
            await release_start.wait()
            client.task_ready = True

        client.finish_task = AsyncMock(side_effect=finish_task)
        client.start_task = AsyncMock(side_effect=start_task)
        extension.client = client
        extension.connected = True
        extension._active_session_id = "old-session"

        send_task = asyncio.create_task(
            extension.send_audio(make_audio_frame(b"pcm"), "session-1")
        )
        await start_started.wait()

        await asyncio.wait_for(extension._send_lock.acquire(), timeout=0.1)
        extension._send_lock.release()

        release_start.set()
        assert await send_task

    asyncio.run(run())


def test_base_emits_ttlw_from_result_reader_task():
    async def run():
        extension = connection_extension()
        client = fake_client()
        release_finish = asyncio.Event()
        finish_started = asyncio.Event()

        async def finish_task():
            finish_started.set()
            await release_finish.wait()
            client.task_ready = False

        client.finish_task = AsyncMock(side_effect=finish_task)
        extension.client = client
        extension.connected = True
        extension.send_asr_result = (
            AsyncASRBaseExtension.send_asr_result.__get__(extension)
        )
        extension._send_metrics_ttlw = AsyncMock()
        extension.send_asr_finalize_end = AsyncMock()

        finalize_data = Data.create("asr_finalize")
        finalize_data.set_property_from_json(
            None,
            json.dumps(
                {
                    "finalize_id": "finalize-1",
                    "metadata": {"session_id": "session-1"},
                }
            ),
        )
        finalize_task = asyncio.create_task(
            extension.on_data(extension.ten_env, finalize_data)
        )
        await finish_started.wait()

        reader_task = asyncio.create_task(
            extension._handle_result(
                {
                    "payload": {
                        "output": {
                            "sentence": {
                                "text": "hello",
                                "sentence_end": True,
                            }
                        }
                    }
                }
            )
        )
        await reader_task

        extension._send_metrics_ttlw.assert_awaited_once()
        assert extension.last_finalize_time is None

        release_finish.set()
        await finalize_task

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


def test_startup_and_stop_complete_while_buffered_send_is_blocked(monkeypatch):
    async def run():
        extension = connection_extension()
        frame = make_audio_frame(b"aaaa")
        await extension._handle_audio_frame(extension.ten_env, frame)
        send_started = asyncio.Event()
        send_cancelled = asyncio.Event()
        client = fake_client()

        async def blocked_send(_audio):
            send_started.set()
            try:
                await asyncio.Event().wait()
            finally:
                send_cancelled.set()

        client.send_audio = AsyncMock(side_effect=blocked_send)
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(return_value=client),
        )

        await asyncio.wait_for(extension.on_start(extension.ten_env), timeout=1)
        await asyncio.wait_for(send_started.wait(), timeout=1)
        assert [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ] == ["connecting", "connected"]

        await asyncio.wait_for(extension.on_stop(extension.ten_env), timeout=1)

        assert send_cancelled.is_set()
        client.close.assert_awaited_once()
        assert [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ] == ["connecting", "connected", "disconnected"]
        assert emitted_payloads(extension, "error") == []
        assert extension._reconnect_task is None
        assert extension._buffer_flush_task is None

    asyncio.run(run())


def test_startup_flush_preserves_order_with_live_audio(monkeypatch):
    async def run():
        extension = connection_extension()
        for audio in (b"aaaa", b"bbbb"):
            await extension._handle_audio_frame(
                extension.ten_env, make_audio_frame(audio)
            )
        send_started = asyncio.Event()
        release_send = asyncio.Event()
        all_sent = asyncio.Event()
        sent = []
        client = fake_client()

        async def send_audio(audio):
            if not sent:
                send_started.set()
                await release_send.wait()
            sent.append(audio)
            if len(sent) == 3:
                all_sent.set()

        client.send_audio = AsyncMock(side_effect=send_audio)
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(return_value=client),
        )

        await asyncio.wait_for(extension.on_start(extension.ten_env), timeout=1)
        await asyncio.wait_for(send_started.wait(), timeout=1)
        await extension._handle_audio_frame(
            extension.ten_env, make_audio_frame(b"cccc")
        )
        release_send.set()
        await asyncio.wait_for(all_sent.wait(), timeout=1)

        assert sent == [b"aaaa", b"bbbb", b"cccc"]
        assert extension.buffered_frames_size == 0
        assert extension.is_connected()
        await extension.on_stop(extension.ten_env)

    asyncio.run(run())


def test_replacement_cancels_blocked_flush_and_preserves_remaining_audio(
    monkeypatch,
):
    async def run():
        extension = connection_extension()
        for audio in (b"aaaa", b"bbbb"):
            await extension._handle_audio_frame(
                extension.ten_env, make_audio_frame(audio)
            )
        send_blocked = asyncio.Event()
        all_sent = asyncio.Event()
        sent = []
        old_client = fake_client()
        new_client = fake_client()

        async def old_send(audio):
            if audio == b"bbbb":
                send_blocked.set()
                await asyncio.Event().wait()
            sent.append(audio)

        async def new_send(audio):
            sent.append(audio)
            if len(sent) == 3:
                all_sent.set()

        old_client.send_audio = AsyncMock(side_effect=old_send)
        new_client.send_audio = AsyncMock(side_effect=new_send)
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(side_effect=[old_client, new_client]),
        )

        await asyncio.wait_for(extension.on_start(extension.ten_env), timeout=1)
        await asyncio.wait_for(send_blocked.wait(), timeout=1)
        await extension._handle_audio_frame(
            extension.ten_env, make_audio_frame(b"cccc")
        )
        await asyncio.wait_for(extension.start_connection(), timeout=1)
        await asyncio.wait_for(all_sent.wait(), timeout=1)

        assert sent == [b"aaaa", b"bbbb", b"cccc"]
        assert extension.buffered_frames_size == 0
        assert extension._buffer_flush_head is None
        old_client.close.assert_awaited_once()
        assert [
            call.args[0] for call in new_client.send_audio.await_args_list
        ] == [b"bbbb", b"cccc"]
        assert emitted_payloads(extension, "error") == []
        await extension.on_stop(extension.ten_env)

    asyncio.run(run())


def test_live_audio_is_sent_while_flushed_audio_dump_is_blocked(monkeypatch):
    async def run():
        extension = connection_extension()
        await extension._handle_audio_frame(
            extension.ten_env, make_audio_frame(b"aaaa")
        )
        dump_started = asyncio.Event()
        release_dump = asyncio.Event()
        live_sent = asyncio.Event()
        client = fake_client()

        async def dump_audio(_audio):
            dump_started.set()
            await release_dump.wait()

        async def send_audio(audio):
            if audio == b"bbbb":
                live_sent.set()

        extension.audio_dumper = MagicMock()
        extension.audio_dumper.push_bytes = AsyncMock(side_effect=dump_audio)
        client.send_audio = AsyncMock(side_effect=send_audio)
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(return_value=client),
        )

        await asyncio.wait_for(extension.on_start(extension.ten_env), timeout=1)
        await asyncio.wait_for(dump_started.wait(), timeout=1)
        live_send = asyncio.create_task(
            extension._handle_audio_frame(
                extension.ten_env, make_audio_frame(b"bbbb")
            )
        )
        await asyncio.wait_for(live_sent.wait(), timeout=1)
        release_dump.set()
        await live_send

        assert [call.args[0] for call in client.send_audio.await_args_list] == [
            b"aaaa",
            b"bbbb",
        ]
        assert extension.buffered_frames_size == 0
        await extension.on_stop(extension.ten_env)

    asyncio.run(run())


def test_post_connect_flush_failure_reports_disconnect_and_reconnects(
    monkeypatch,
):
    async def run():
        extension = connection_extension()
        frame = AudioFrame.create("pcm_frame")
        frame.alloc_buf(320)
        extension.buffered_frames.put_nowait(frame)
        extension.buffered_frames_size = 320

        client = fake_client()
        client.send_audio = AsyncMock(
            side_effect=QwenProtocolError("timeout", "timeout")
        )
        monkeypatch.setattr(
            "qwen_asr_python.extension.QwenASRClient",
            MagicMock(return_value=client),
        )

        await extension.start_connection()
        await extension._buffer_flush_task
        if extension._task_recovery_task is not None:
            await extension._task_recovery_task

        statuses = [
            event["current"]
            for event in emitted_payloads(
                extension, "connection_status_changed"
            )
        ]
        assert statuses == ["connecting", "connected", "disconnected"]
        assert extension._buffer_flush_head is frame
        assert extension.buffered_frames_size == 320
        assert extension._reconnect_task is not None
        extension._reconnect_task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await extension._reconnect_task

    asyncio.run(run())


def test_flush_send_false_preserves_head_without_clearing():
    async def run():
        extension = connection_extension()
        frame = AudioFrame.create("pcm_frame")
        frame.alloc_buf(4)
        extension.buffered_frames.put_nowait(frame)
        extension.buffered_frames_size = 4

        client = fake_client()
        client.send_audio = AsyncMock(return_value=None)
        extension.client = client
        extension.connected = True

        extension._send_audio_locked = AsyncMock(return_value=False)

        await extension._flush_buffered_frames_locked()

        assert extension._buffer_flush_head is frame
        assert extension.buffered_frames.empty()
        assert extension.buffered_frames_size == 4

    asyncio.run(run())


def test_flush_send_failure_keeps_failed_frame_as_head(monkeypatch):
    async def run():
        extension = connection_extension()
        first = AudioFrame.create("pcm_frame")
        first.alloc_buf(4)
        first.get_buf()[:] = b"aaaa"
        second = AudioFrame.create("pcm_frame")
        second.alloc_buf(4)
        second.get_buf()[:] = b"bbbb"
        extension.buffered_frames.put_nowait(first)
        extension.buffered_frames.put_nowait(second)
        extension.buffered_frames_size = 8

        client = fake_client()
        client.send_audio = AsyncMock(
            side_effect=[
                None,
                QwenProtocolError("timeout", "timeout"),
            ]
        )
        extension.client = client
        extension.connected = True
        extension._generation = 1

        with pytest.raises(QwenProtocolError):
            await extension._flush_buffered_frames_locked()

        assert extension._buffer_flush_head is second
        assert extension.buffered_frames.empty()
        assert extension.buffered_frames_size == 4
        assert client.send_audio.await_count == 2

    asyncio.run(run())


def test_flush_failure_recovery_sends_each_buffered_frame_once():
    async def run():
        extension = connection_extension()
        first = make_audio_frame(b"aaaa")
        second = make_audio_frame(b"bbbb")
        extension.buffered_frames.put_nowait(first)
        extension.buffered_frames.put_nowait(second)
        extension.buffered_frames_size = 8

        client = fake_client()
        client.send_audio = AsyncMock(
            side_effect=[
                None,
                QwenProtocolError("timeout", "timeout"),
                None,
            ]
        )
        extension.client = client
        extension.connected = True
        extension._generation = 1

        with pytest.raises(QwenProtocolError):
            await extension._flush_buffered_frames_locked()

        assert extension._buffer_flush_head is second
        assert extension.buffered_frames_size == 4

        flushed = await extension._flush_buffered_frames_locked()
        assert extension._buffer_flush_head is None
        assert flushed == [b"bbbb"]
        assert extension.buffered_frames.empty()
        assert extension.buffered_frames_size == 0
        assert client.send_audio.await_count == 3

    asyncio.run(run())


def test_handle_audio_frame_buffers_during_finalize_finish():
    async def run():
        extension = connection_extension()
        client = fake_client()
        release_finish = asyncio.Event()
        finish_started = asyncio.Event()

        async def finish_task():
            finish_started.set()
            await release_finish.wait()
            client.task_ready = False

        client.finish_task = AsyncMock(side_effect=finish_task)
        extension.client = client
        extension.connected = True

        finalize_task = asyncio.create_task(extension.finalize("session-1"))
        await finish_started.wait()

        frame = make_audio_frame(b"\x01\x02\x03\x04")
        await extension._handle_audio_frame(extension.ten_env, frame)
        assert extension.buffered_frames.qsize() == 1
        assert extension.buffered_frames_size == 4
        client.send_audio.assert_not_awaited()

        release_finish.set()
        await finalize_task
        if extension._task_start_task is not None:
            await extension._task_start_task

    asyncio.run(run())


def test_send_audio_rebuffers_when_blocked_on_finalize_transition():
    async def run():
        extension = connection_extension()
        client = fake_client()
        release_finish = asyncio.Event()
        finish_started = asyncio.Event()

        async def finish_task():
            finish_started.set()
            await release_finish.wait()
            client.task_ready = False

        client.finish_task = AsyncMock(side_effect=finish_task)
        extension.client = client
        extension.connected = True

        finalize_task = asyncio.create_task(extension.finalize("session-1"))
        await finish_started.wait()

        frame = make_audio_frame(b"\x05\x06")
        send_task = asyncio.create_task(
            extension.send_audio(frame, "session-1")
        )
        await asyncio.sleep(0)
        release_finish.set()
        assert await send_task is False
        assert (
            extension._buffer_flush_head is frame
            or extension.buffered_frames.qsize() == 1
        )
        assert extension.buffered_frames_size == 2
        await finalize_task
        if extension._task_start_task is not None:
            await extension._task_start_task

    asyncio.run(run())


def test_finalize_clears_last_finalize_time_when_handshake_completes():
    async def run():
        extension = connection_extension()
        client = fake_client()
        extension.client = client
        extension.connected = True
        extension.send_asr_result = (
            AsyncASRBaseExtension.send_asr_result.__get__(extension)
        )
        extension._send_metrics_ttlw = AsyncMock()

        await extension._handle_result(
            {
                "payload": {
                    "output": {
                        "sentence": {
                            "text": "auto final",
                            "sentence_end": True,
                        }
                    }
                }
            }
        )
        extension._send_metrics_ttlw.assert_not_awaited()

        loop = asyncio.get_event_loop()
        extension.last_finalize_time = loop.time() - 10.0
        await extension.finalize("session-1")
        if extension._task_start_task is not None:
            await extension._task_start_task

        assert extension.last_finalize_time is None
        extension._send_metrics_ttlw.assert_not_awaited()

        base_time = loop.time()
        extension.last_finalize_time = base_time
        real_time = loop.time

        def later_time():
            return base_time + 0.05

        loop.time = later_time
        try:
            await extension._handle_result(
                {
                    "payload": {
                        "output": {
                            "sentence": {
                                "text": "turn two",
                                "sentence_end": True,
                            }
                        }
                    }
                }
            )
        finally:
            loop.time = real_time

        extension._send_metrics_ttlw.assert_awaited_once()
        ttlw = extension._send_metrics_ttlw.await_args.args[0]
        assert 0 <= ttlw < 100

    asyncio.run(run())


def test_stale_recovery_skips_disconnect_after_replacement(monkeypatch):
    async def run():
        extension = connection_extension()
        old_client = fake_client()
        new_client = fake_client()
        extension.client = old_client
        extension.connected = True
        extension._generation = 2
        extension.on_disconnected = AsyncMock()
        extension._schedule_reconnect = MagicMock()

        async def slow_error(*_args, **_kwargs):
            extension.client = new_client
            extension.connected = True
            extension._generation = 3
            await asyncio.sleep(0)

        extension.send_asr_error = AsyncMock(side_effect=slow_error)

        await extension._recover_client(
            old_client, QwenProtocolError("timeout", "timeout"), False
        )

        extension.on_disconnected.assert_not_awaited()
        extension._schedule_reconnect.assert_not_called()

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
        await extension._buffer_flush_task

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


def test_task_finished_reports_vendor_usage():
    async def run():
        extension = connection_extension()
        extension.send_vendor_metrics = AsyncMock()

        await extension._handle_event(
            {
                "header": {"event": "task-finished"},
                "payload": {
                    "usage": {
                        "duration": 9,
                        "input_tokens": 365,
                        "output_tokens": 12,
                        "total_tokens": 377,
                    }
                },
            },
            extension._generation,
        )

        extension.send_vendor_metrics.assert_awaited_once_with(
            {
                "usage": {
                    "duration": 9,
                    "input_tokens": 365,
                    "output_tokens": 12,
                    "total_tokens": 377,
                }
            }
        )

    asyncio.run(run())


def test_final_result_reports_usage_and_task_finished_does_not_duplicate():
    async def run():
        extension = connection_extension()
        extension.send_vendor_metrics = AsyncMock()
        usage = {
            "duration": 3,
            "input_tokens": 23,
            "output_tokens": 6,
            "total_tokens": 29,
        }

        await extension._handle_event(
            {
                "header": {
                    "event": "result-generated",
                    "task_id": "task-1",
                },
                "payload": {
                    "output": {
                        "sentence": {
                            "text": "hello",
                            "sentence_end": True,
                        }
                    },
                    "usage": usage,
                },
            },
            extension._generation,
        )
        await extension._handle_event(
            {
                "header": {
                    "event": "task-finished",
                    "task_id": "task-1",
                },
                "payload": {"usage": usage},
            },
            extension._generation,
        )

        extension.send_vendor_metrics.assert_awaited_once_with({"usage": usage})

    asyncio.run(run())


def test_task_finished_without_usage_does_not_report_vendor_metrics():
    async def run():
        extension = connection_extension()
        extension.send_vendor_metrics = AsyncMock()

        await extension._handle_event(
            {"header": {"event": "task-finished"}, "payload": {}},
            extension._generation,
        )

        extension.send_vendor_metrics.assert_not_awaited()

    asyncio.run(run())


def connection_extension():
    extension = configured_extension()
    extension.ten_env.send_data = AsyncMock()
    return extension


def make_audio_frame(audio_data: bytes) -> AudioFrame:
    frame = AudioFrame.create("pcm_frame")
    frame.alloc_buf(len(audio_data))
    buf = frame.lock_buf()
    buf[:] = audio_data
    frame.unlock_buf(buf)
    return frame


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


@pytest.mark.parametrize(
    "final,vendor_language,expected_language",
    [
        (False, None, "en-US"),
        (True, None, "en-US"),
        (True, "ja", "ja-JP"),
    ],
    ids=["interim-fallback", "final-fallback", "vendor-language"],
)
def test_emitted_result_language_with_nested_language_config(
    final, vendor_language, expected_language
):
    async def run():
        extension = configured_extension()
        extension.ten_env.get_property_bool = AsyncMock(
            return_value=(False, None)
        )
        extension.ten_env.get_property_to_json = AsyncMock(
            return_value=(
                json.dumps(
                    {
                        "language": "zh-CN",
                        "params": {
                            "api_key": "secret",
                            "language": "en-US",
                            "language_hints": ["en"],
                        },
                    }
                ),
                None,
            )
        )
        await extension.on_init(extension.ten_env)
        extension.metadata = {
            "session_id": "100",
            "source_languages": "en-US",
        }
        extension.ten_env.send_data = AsyncMock()
        extension.send_asr_result = (
            AsyncASRBaseExtension.send_asr_result.__get__(extension)
        )
        sentence = {
            "text": "The silver train carries eleven small boxes.",
            "sentence_end": final,
        }
        if vendor_language is not None:
            sentence["language"] = vendor_language

        await extension._handle_result(
            {"payload": {"output": {"sentence": sentence}}}
        )

        emitted = [
            call.args[0]
            for call in extension.ten_env.send_data.await_args_list
            if call.args[0].get_name() == "asr_result"
        ]
        assert len(emitted) == 1
        payload = json.loads(emitted[0].get_property_to_json(None)[0])
        assert payload["text"] == sentence["text"]
        assert payload["final"] is final
        assert payload["language"] == expected_language
        assert payload["metadata"]["source_languages"] == "en-US"
        assert payload["metadata"]["session_id"] == "100"

    asyncio.run(run())


def test_nested_timeout_is_validated_during_initialization():
    async def run():
        extension = QwenASRExtension("qwen_asr_python")
        ten_env = MagicMock()
        ten_env.get_property_bool = AsyncMock(return_value=(False, None))
        ten_env.get_property_to_json = AsyncMock(
            return_value=(
                json.dumps(
                    {"params": {"api_key": "secret", "connect_timeout_s": 0}}
                ),
                None,
            )
        )
        ten_env.send_data = AsyncMock()

        await extension.on_init(ten_env)

        errors = [
            json.loads(call.args[0].get_property_to_json(None)[0])
            for call in ten_env.send_data.await_args_list
            if call.args[0].get_name() == "error"
        ]
        assert len(errors) == 1
        assert errors[0]["code"] == int(ModuleErrorCode.FATAL_ERROR.value)

    asyncio.run(run())
