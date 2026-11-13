"""Exercise actual TEN reporting, with only the provider transport mocked."""

import asyncio
import copy
import json
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from ten_ai_base.message import ModuleErrorCode
from ten_runtime import Data
from websockets.exceptions import ConnectionClosedError

from speko_asr_python.client import SpekoCommitTimeout, SpekoRouterError
from speko_asr_python.config import SpekoASRConfig
from speko_asr_python.extension import SpekoASRExtension


def extension_and_events():
    extension = SpekoASRExtension("speko")
    extension.config = SpekoASRConfig(api_key="test-secret-key-long")
    extension.ten_env = MagicMock()
    events = []

    async def send(data):
        raw, _ = data.get_property_to_json("")
        events.append((data.get_name(), json.loads(raw)))

    extension.ten_env.send_data = AsyncMock(side_effect=send)
    extension.client = MagicMock(
        is_ready=True,
        send_audio=AsyncMock(),
        commit=AsyncMock(),
        close=AsyncMock(),
    )
    return extension, events


def audio(metadata=None, size=32000):
    frame = MagicMock()
    frame.get_buf.return_value = bytes(size)
    frame.lock_buf.return_value = bytes(size)
    frame.get_property_to_json.return_value = (
        json.dumps(metadata or {"session_id": "audio-session"}),
        None,
    )
    return frame


def finalize_data(finalize_id, session_id):
    data = Data.create("asr_finalize")
    data.set_property_string("finalize_id", finalize_id)
    data.set_property_from_json(
        "metadata",
        json.dumps({"session_id": session_id, "custom": finalize_id}),
    )
    return data


def payloads(events, name):
    return [payload for kind, payload in events if kind == name]


@pytest.mark.asyncio
async def test_emitted_results_preserve_metadata_and_ids_without_empty_turns():
    ext, events = extension_and_events()
    metadata = {
        "session_id": "audio-session",
        "custom": "keep",
        "asr_info": {"caller": 1},
    }
    await ext._handle_audio_frame(ext.ten_env, audio(metadata))
    original = copy.deepcopy(ext.metadata)
    ext.first_audio_time = asyncio.get_running_loop().time() - 0.1
    for kind, text in (
        ("transcript.delta", "hello"),
        ("transcript.final", "hello"),
        ("transcript.final", ""),
        ("transcript.delta", " "),
    ):
        await ext._on_router_event({"type": kind, "text": text, "speaker": "A"})
    results = payloads(events, "asr_result")
    assert len(results) == 2
    assert results[0]["id"] == results[1]["id"]
    assert results[1]["final"] is True
    assert results[1]["language"] == "en-US"
    assert "session_id" not in results[1]
    assert results[1]["metadata"] == {
        **metadata,
        "asr_info": {
            "caller": 1,
            "vendor": "speko",
            "locked": False,
            "speaker": "A",
        },
    }
    assert ext.metadata == original
    metrics = payloads(events, "metrics")
    assert metrics and metrics[0]["metadata"]["session_id"] == "audio-session"


@pytest.mark.asyncio
async def test_concurrent_finalize_echoes_each_request_and_blocks_later_audio():
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())
    entered, release = asyncio.Event(), asyncio.Event()

    async def commit(_context=None):
        entered.set()
        await release.wait()
        await ext._on_router_event({"type": "transcript.final", "text": "done"})

    ext.client.commit.side_effect = commit
    first = asyncio.create_task(
        ext.on_data(ext.ten_env, finalize_data("one", "session-one"))
    )
    await entered.wait()
    second = asyncio.create_task(
        ext.on_data(ext.ten_env, finalize_data("two", "session-two"))
    )
    later_audio = asyncio.create_task(
        ext._handle_audio_frame(ext.ten_env, audio({"session_id": "next"}))
    )
    await asyncio.sleep(0)
    assert ext.client.commit.await_count == 1
    assert not later_audio.done()
    release.set()
    await asyncio.wait_for(asyncio.gather(first, second, later_audio), 1)
    acknowledgements = payloads(events, "asr_finalize_end")
    assert [
        (item["finalize_id"], item["metadata"]["session_id"])
        for item in acknowledgements
    ] == [("one", "session-one"), ("two", "session-two")]
    assert all(
        item["metadata"]["custom"] == item["finalize_id"]
        for item in acknowledgements
    )
    kinds = [kind for kind, _ in events]
    assert kinds.index("asr_result") < kinds.index("asr_finalize_end")
    assert ext.last_finalize_time is None


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "failure,errors",
    [
        (None, 0),
        (SpekoCommitTimeout("request_timeout", "timeout", retryable=True), 0),
        (
            SpekoRouterError(
                "relay_error",
                "Speko ASR session closed before final transcript",
                retryable=True,
            ),
            1,
        ),
        (OSError("closed"), 1),
    ],
)
async def test_finalize_acknowledges_disconnection_and_errors(failure, errors):
    ext, events = extension_and_events()
    if failure is None:
        ext.client = None
    else:
        ext.client.commit.side_effect = failure
    await ext.on_data(ext.ten_env, finalize_data("id", "final-session"))
    await ext.stop_connection()
    assert payloads(events, "asr_finalize_end") == [
        {
            "finalize_id": "id",
            "metadata": {"session_id": "final-session", "custom": "id"},
        }
    ]
    assert len(payloads(events, "error")) == errors


@pytest.mark.asyncio
async def test_commit_timeout_keeps_session_and_next_turn_timeline():
    ext, events = extension_and_events()
    client = ext.client
    await ext._handle_audio_frame(ext.ten_env, audio())
    client.commit.side_effect = SpekoCommitTimeout(
        "request_timeout",
        "Timed out waiting for a final Speko transcript",
        retryable=True,
    )
    await ext.on_data(ext.ten_env, finalize_data("one", "turn-one"))
    assert ext.client is client and ext.is_connected()
    assert ext._reconnect_task is None
    client.close.assert_not_awaited()
    assert not payloads(events, "error")
    assert not payloads(events, "connection_status_changed")
    assert [
        item["finalize_id"] for item in payloads(events, "asr_finalize_end")
    ] == ["one"]
    warnings = [call.args[0] for call in ext.ten_env.log_warn.call_args_list]
    assert any("not acknowledged" in message for message in warnings)
    # The next turn starts where the timed-out finalize closed the last one.
    client.commit.side_effect = None
    await ext._handle_audio_frame(ext.ten_env, audio())
    await ext._on_router_event({"type": "transcript.final", "text": "next"})
    await ext.on_data(ext.ten_env, finalize_data("two", "turn-two"))
    results = payloads(events, "asr_result")
    assert [(item["start_ms"], item["duration_ms"]) for item in results] == [
        (1000, 1000)
    ]
    assert len(payloads(events, "asr_finalize_end")) == 2
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
async def test_empty_final_acknowledges_finalize_without_a_result():
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())

    async def commit(_context=None):
        await ext._on_router_event({"type": "transcript.final", "text": ""})

    ext.client.commit.side_effect = commit
    await ext.on_data(ext.ten_env, finalize_data("empty", "turn"))
    assert not payloads(events, "asr_result")
    assert not payloads(events, "error")
    assert [
        item["finalize_id"] for item in payloads(events, "asr_finalize_end")
    ] == ["empty"]
    assert ext._final_cursor_ms == 1000
    await ext.on_stop(ext.ten_env)


def ttlw_metrics(events):
    return [
        payload
        for payload in payloads(events, "metrics")
        if "ttlw" in payload["metrics"]
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "final_text,session",
    [(None, "turn-one"), ("", "turn-one"), ("done", "audio-session")],
    ids=["no_commit_needed", "empty_final_ack", "final_result"],
)
async def test_acknowledged_finalize_reports_ttlw_once(final_text, session):
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())

    async def commit(_context=None):
        if final_text is not None:
            await ext._on_router_event(
                {"type": "transcript.final", "text": final_text}
            )

    ext.client.commit.side_effect = commit
    await ext.on_data(ext.ten_env, finalize_data("one", "turn-one"))
    ttlw = ttlw_metrics(events)
    assert len(ttlw) == 1
    assert ttlw[0]["metrics"]["ttlw"] >= 0
    # Without a result, TTLW belongs to the finalize it measures, like its
    # acknowledgement; with one, to that result's session.
    assert ttlw[0]["metadata"]["session_id"] == session
    end = payloads(events, "asr_finalize_end")[0]
    assert end["metadata"]["session_id"] == "turn-one"
    kinds = [
        "ttlw" if kind == "metrics" and "ttlw" in payload["metrics"] else kind
        for kind, payload in events
    ]
    assert kinds.index("ttlw") < kinds.index("asr_finalize_end")
    assert len(payloads(events, "asr_result")) == (1 if final_text else 0)
    assert ext.last_finalize_time is None
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "failure",
    [
        SpekoCommitTimeout("request_timeout", "timeout", retryable=True),
        SpekoRouterError("relay_error", "closed", retryable=True),
    ],
    ids=["commit_timeout", "disconnect"],
)
async def test_unacknowledged_finalize_reports_no_ttlw(failure):
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())
    ext.client.commit.side_effect = failure
    await ext.on_data(ext.ten_env, finalize_data("one", "turn-one"))
    await ext.stop_connection()
    assert not ttlw_metrics(events)
    assert len(payloads(events, "asr_finalize_end")) == 1
    assert ext.last_finalize_time is None


def _upgrade_rejected(status):
    from websockets.exceptions import InvalidStatus

    response = MagicMock(status_code=status, reason_phrase="denied", body=b"")
    return AsyncMock(side_effect=InvalidStatus(response))


class _ErrorBeforeReady:
    """A Router socket that rejects session.configure with an error event."""

    def __init__(self, retryable):
        self.retryable = retryable
        self.closed = False

    async def send(self, _value):
        pass

    async def recv(self):
        return json.dumps(
            {
                "type": "error",
                "error": {
                    "code": "invalid_request",
                    "message": "unsupported language",
                    "retryable": self.retryable,
                },
            }
        )

    async def close(self):
        self.closed = True


def _error_before_ready(retryable):
    return AsyncMock(return_value=_ErrorBeforeReady(retryable))


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "connect,router_code",
    [
        (_upgrade_rejected(400), "invalid_request"),
        (_error_before_ready(False), "invalid_request"),
    ],
)
async def test_non_retryable_setup_failure_is_one_fatal_error_without_retry(
    connect, router_code
):
    ext, events = extension_and_events()
    ext.client = None
    ext.ten_env.get_property_bool = AsyncMock(return_value=(True, None))
    ext.ten_env.get_property_to_json = AsyncMock(
        return_value=(json.dumps({"params": {"api_key": "key"}}), None)
    )
    with patch("speko_asr_python.client.websockets.connect", connect):
        await ext.on_init(ext.ten_env)
        await ext.on_start(ext.ten_env)
        await ext._ensure_connection()
        for _ in range(3):
            await ext.on_audio_frame(ext.ten_env, audio())
            await ext.on_data(ext.ten_env, Data.create("trigger_connect"))
        await asyncio.sleep(0)
        assert ext._reconnect_task is None
        assert connect.await_count == 1
        await ext.on_stop(ext.ten_env)
    errors = payloads(events, "error")
    assert len(errors) == 1
    assert errors[0]["code"] == int(ModuleErrorCode.FATAL_ERROR.value)
    assert errors[0]["vendor_info"]["code"] == router_code
    assert ext._permanent_error is not None
    assert not ext._retired_clients


@pytest.mark.asyncio
async def test_caller_queued_behind_fatal_handshake_emits_no_transition():
    ext, events = extension_and_events()
    ext.client = None
    entered, release = asyncio.Event(), asyncio.Event()
    rejected = _upgrade_rejected(401)

    async def connect(*args, **kwargs):
        entered.set()
        await release.wait()
        return await rejected(*args, **kwargs)

    with patch("speko_asr_python.client.websockets.connect", connect):
        first = asyncio.create_task(ext._ensure_connection())
        await entered.wait()
        # Passes the unlocked latch check, then queues on _connection_lock.
        queued = asyncio.create_task(ext._ensure_connection())
        await asyncio.sleep(0)
        release.set()
        await asyncio.gather(first, queued)
    statuses = [
        (status["current"], status["code"])
        for status in payloads(events, "connection_status_changed")
    ]
    assert statuses == [
        ("connecting", 0),
        ("disconnected", int(ModuleErrorCode.FATAL_ERROR.value)),
    ]
    assert len(payloads(events, "error")) == 1
    assert rejected.await_count == 1


class _RouterSocket:
    """A Router socket that is ready, streams queued events, then closes."""

    def __init__(self, failure=None, *, commit_ids=False):
        self.failure = failure
        self.commit_ids = commit_ids
        self.events = asyncio.Queue()
        self.sent = []

    async def send(self, value):
        self.sent.append(value)

    async def recv(self):
        ready = {"type": "session.ready"}
        if self.commit_ids:
            ready["commit_ids"] = True
        return json.dumps(ready)

    def commits(self):
        """commit_id of every input.commit sent, in order."""
        return [
            json.loads(value)["commit_id"]
            for value in self.sent
            if isinstance(value, str)
            and json.loads(value).get("type") == "input.commit"
        ]

    def __aiter__(self):
        return self

    async def __anext__(self):
        event = await self.events.get()
        if event is None:
            if self.failure is not None:
                raise self.failure
            raise StopAsyncIteration
        return json.dumps(event)

    async def close(self):
        pass


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "failure",
    [None, ConnectionClosedError(None, None)],
    ids=["normal_close", "abnormal_close"],
)
async def test_only_an_abnormal_close_reports_an_error(failure):
    ext, events = extension_and_events()
    ext.client = None
    ext._schedule_reconnect = MagicMock()
    socket = _RouterSocket(failure)
    with patch(
        "speko_asr_python.client.websockets.connect",
        AsyncMock(return_value=socket),
    ):
        await ext.start_connection()
        socket.events.put_nowait(None)
        for _ in range(50):
            if ext._schedule_reconnect.called:
                break
            await asyncio.sleep(0)
    ext._schedule_reconnect.assert_called_once()
    statuses = [
        (status["current"], status["code"])
        for status in payloads(events, "connection_status_changed")
    ]
    if failure is None:
        assert statuses[-1] == ("disconnected", 0)
        assert not payloads(events, "error")
    else:
        code = int(ModuleErrorCode.NON_FATAL_ERROR.value)
        assert statuses[-1] == ("disconnected", code)
        assert [error["code"] for error in payloads(events, "error")] == [code]
    await ext.on_stop(ext.ten_env)


async def _wait_for(predicate):
    for _ in range(100):
        if predicate():
            return
        await asyncio.sleep(0)
    raise AssertionError("condition not reached")


async def _stream(ext, session):
    await ext.on_audio_frame(ext.ten_env, audio({"session_id": session}))
    await ext._handle_audio_frame(
        ext.ten_env, ext.audio_frames_queue.get_nowait()
    )


def _results_and_ends(events):
    return [
        payload.get("finalize_id") or payload.get("text")
        for kind, payload in events
        if kind in {"asr_result", "asr_finalize_end"}
    ]


@pytest.mark.asyncio
async def test_late_final_keeps_its_turn_and_next_finalize_waits_for_its_own():
    ext, events = extension_and_events()
    ext.client = None
    ext.config.finalize_timeout_sec = 0.05
    socket = _RouterSocket(commit_ids=True)
    with patch(
        "speko_asr_python.client.websockets.connect",
        AsyncMock(return_value=socket),
    ):
        await ext.start_connection()
        await _stream(ext, "one")
        # Turn one's commit times out; its final is still on the way.
        await ext.on_data(ext.ten_env, finalize_data("one", "one"))
        ext.client.finalize_timeout_sec = 1
        await _stream(ext, "two")
        second = asyncio.create_task(
            ext.on_data(ext.ten_env, finalize_data("two", "two"))
        )
        await _wait_for(lambda: len(socket.commits()) == 2)
        first_id, second_id = socket.commits()
        socket.events.put_nowait(
            {"type": "transcript.final", "text": "late", "commit_id": first_id}
        )
        await _wait_for(lambda: payloads(events, "asr_result"))
        assert not second.done()
        socket.events.put_nowait(
            {"type": "transcript.final", "text": "own", "commit_id": second_id}
        )
        await asyncio.wait_for(second, 1)
    results = payloads(events, "asr_result")
    assert [
        (item["text"], item["metadata"]["session_id"]) for item in results
    ] == [
        ("late", "one"),
        ("own", "two"),
    ]
    assert _results_and_ends(events) == ["one", "late", "own", "two"]
    assert not payloads(events, "error")
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
@pytest.mark.parametrize("segments", [False, True], ids=["text", "segments"])
async def test_late_final_keeps_its_turns_id_timeline_and_ttlw(segments):
    ext, events = extension_and_events()
    ext.client = None
    ext.config.finalize_timeout_sec = 0.05
    socket = _RouterSocket(commit_ids=True)

    def answer(text, commit_id, start_ms, end_ms):
        event = {
            "type": "transcript.final",
            "text": text,
            "commit_id": commit_id,
        }
        if segments:
            event["segments"] = [
                {"start_ms": start_ms, "end_ms": end_ms, "text": text}
            ]
        return event

    def results():
        return payloads(events, "asr_result")

    with patch(
        "speko_asr_python.client.websockets.connect",
        AsyncMock(return_value=socket),
    ):
        await ext.start_connection()
        await _stream(ext, "one")  # 0-1000 ms
        socket.events.put_nowait({"type": "transcript.delta", "text": "la"})
        await _wait_for(lambda: len(results()) == 1)
        # Turn one's commit times out; its final is still on the way.
        await ext.on_data(ext.ten_env, finalize_data("one", "one"))
        ext.client.finalize_timeout_sec = 1
        await _stream(ext, "two")  # 1000-2000 ms
        socket.events.put_nowait({"type": "transcript.delta", "text": "tw"})
        await _wait_for(lambda: len(results()) == 2)
        second = asyncio.create_task(
            ext.on_data(ext.ten_env, finalize_data("two", "two"))
        )
        await _wait_for(lambda: len(socket.commits()) == 2)
        first_id, second_id = socket.commits()
        socket.events.put_nowait(answer("late", first_id, 0, 800))
        await _wait_for(lambda: len(results()) == 3)
        assert not second.done()
        socket.events.put_nowait(answer("own", second_id, 1000, 2000))
        await asyncio.wait_for(second, 1)
    early, interim, late, own = results()
    # The late final closes turn one: its id, metadata and audio.
    assert late["final"] and late["id"] == early["id"]
    assert late["metadata"] == early["metadata"]
    assert late["metadata"]["session_id"] == "one"
    assert (late["start_ms"], late["duration_ms"]) == (
        (0, 800) if segments else (0, 1000)
    )
    # Turn two's interim is closed by turn two's own final, not by turn one's.
    assert own["final"] and own["id"] == interim["id"] != late["id"]
    assert interim["metadata"]["session_id"] == "two"
    assert own["metadata"]["session_id"] == "two"
    assert (own["start_ms"], own["duration_ms"]) == (1000, 1000)
    # Turn two's own final reports its TTLW; one finalize_end per finalize.
    order = [
        (
            "ttlw"
            if kind == "metrics"
            else payload.get("finalize_id") or payload.get("text")
        )
        for kind, payload in events
        if kind in {"asr_result", "asr_finalize_end"}
        or (kind == "metrics" and "ttlw" in payload["metrics"])
    ]
    assert order == ["la", "one", "tw", "late", "ttlw", "own", "two"]
    assert [
        item["metadata"]["session_id"] for item in ttlw_metrics(events)
    ] == ["two"]
    assert not payloads(events, "error")
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
async def test_missing_first_answer_then_second_turn_completes_on_its_own():
    ext, events = extension_and_events()
    ext.client = None
    ext.config.finalize_timeout_sec = 0.05
    socket = _RouterSocket(commit_ids=True)
    with patch(
        "speko_asr_python.client.websockets.connect",
        AsyncMock(return_value=socket),
    ):
        await ext.start_connection()
        await _stream(ext, "one")
        # Turn one's answer never arrives.
        await ext.on_data(ext.ten_env, finalize_data("one", "one"))
        ext.client.finalize_timeout_sec = 1
        await _stream(ext, "two")
        second = asyncio.create_task(
            ext.on_data(ext.ten_env, finalize_data("two", "two"))
        )
        await _wait_for(lambda: len(socket.commits()) == 2)
        socket.events.put_nowait(
            {
                "type": "transcript.final",
                "text": "two",
                "commit_id": socket.commits()[1],
            }
        )
        await asyncio.wait_for(second, 1)
    assert [
        (item["text"], item["metadata"]["session_id"])
        for item in payloads(events, "asr_result")
    ] == [("two", "two")]
    assert _results_and_ends(events) == ["one", "two", "two"]
    # The same session served both turns.
    assert len(payloads(events, "connection_status_changed")) == 2
    assert not payloads(events, "error")
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
async def test_without_commit_ids_a_missing_answer_resets_and_next_turn_completes():
    ext, events = extension_and_events()
    ext.client = None
    ext.config.finalize_timeout_sec = 0.05
    ext.RECONNECT_DELAYS = (0.0,)
    first, second = _RouterSocket(), _RouterSocket()
    with patch(
        "speko_asr_python.client.websockets.connect",
        AsyncMock(side_effect=[first, second]),
    ):
        await ext.start_connection()
        await _stream(ext, "one")
        # An older Router answers in order; this answer never arrives.
        await ext.on_data(ext.ten_env, finalize_data("one", "one"))
        await _wait_for(ext.is_connected)
        ext.client.finalize_timeout_sec = 1
        await _stream(ext, "two")
        turn = asyncio.create_task(
            ext.on_data(ext.ten_env, finalize_data("two", "two"))
        )
        await _wait_for(second.commits)
        second.events.put_nowait({"type": "transcript.final", "text": "two"})
        await asyncio.wait_for(turn, 1)
    assert len(first.commits()) == 1
    assert [
        (item["text"], item["metadata"]["session_id"])
        for item in payloads(events, "asr_result")
    ] == [("two", "two")]
    assert _results_and_ends(events) == ["one", "two", "two"]
    # One non-fatal error for the reset, then a fresh session.
    assert [error["code"] for error in payloads(events, "error")] == [
        int(ModuleErrorCode.NON_FATAL_ERROR.value)
    ]
    assert [
        status["current"]
        for status in payloads(events, "connection_status_changed")
    ] == ["connecting", "connected", "disconnected", "connecting", "connected"]
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
async def test_retryable_setup_failure_reports_non_fatal_and_reconnects():
    ext, events = extension_and_events()
    ext.client = None
    ext.config = SpekoASRConfig(api_key="key")
    with patch(
        "speko_asr_python.client.websockets.connect", _upgrade_rejected(503)
    ):
        await ext.start_connection()
    errors = payloads(events, "error")
    assert len(errors) == 1
    assert errors[0]["code"] == int(ModuleErrorCode.NON_FATAL_ERROR.value)
    assert ext._permanent_error is None
    assert ext._reconnect_task is not None
    await ext.stop_connection()


@pytest.mark.asyncio
async def test_real_status_events_redact_key_and_url():
    ext, events = extension_and_events()
    ext.config.base_url = "https://alice:password@example.com/router?token=private&signature=signed#secret"
    clients = []

    def factory(**kwargs):
        client = MagicMock(is_ready=True, close=AsyncMock())

        async def connect():
            await kwargs["on_event"]({"type": "session.ready"})

        client.connect = AsyncMock(side_effect=connect)
        clients.append((client, kwargs))
        return client

    with patch(
        "speko_asr_python.extension.SpekoASRClient", side_effect=factory
    ):
        await ext.start_connection()
        await ext.start_connection()
        before = list(events)
        await clients[0][1]["on_disconnect"](
            SpekoRouterError("relay_error", "old", retryable=True)
        )
        await clients[0][1]["on_event"](
            {"type": "transcript.final", "text": "stale"}
        )
        assert events == before
        assert ext.client is clients[1][0]
        await ext.on_stop(ext.ten_env)
    statuses = payloads(events, "connection_status_changed")
    assert [item["current"] for item in statuses] == [
        "connecting",
        "connected",
        "connecting",
        "connected",
        "disconnected",
    ]
    serialized = json.dumps(events) + ext.config.to_str()
    for secret in (
        "test-secret-key-long",
        "alice",
        "password",
        "private",
        "signed",
        "#secret",
    ):
        assert secret not in serialized
    vendor = statuses[1]["metadata"]["vendor_metadata"]
    assert vendor["base_url"] == "https://example.com/router"
    assert "key" in vendor and "api_key" not in vendor


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "params",
    [
        {"api_key": "private-value", "base_url": "invalid"},
        {"api_key": "private-value", "buffer_duration_ms": 30001},
    ],
)
async def test_invalid_configuration_is_latched_before_any_connect_attempt(
    params,
):
    ext, events = extension_and_events()
    ext.client = None
    ext.ten_env.get_property_bool = AsyncMock(return_value=(True, None))
    ext.ten_env.get_property_to_json = AsyncMock(
        return_value=(json.dumps({"params": params}), None)
    )
    # The real base initializer starts a consumer; stop it through the lifecycle.
    with patch("speko_asr_python.extension.SpekoASRClient") as constructor:
        await ext.on_init(ext.ten_env)
        await ext.on_start(ext.ten_env)
        for _ in range(3):
            await ext.on_audio_frame(ext.ten_env, audio())
            await ext.on_data(ext.ten_env, Data.create("trigger_connect"))
        await ext.on_stop(ext.ten_env)
        constructor.assert_not_called()
    assert len(payloads(events, "error")) == 1
    assert payloads(events, "error")[0]["code"] == int(
        ModuleErrorCode.FATAL_ERROR.value
    )
    assert "private-value" not in json.dumps(events)


async def _reconnect_with_recorded_delays(ext):
    real_sleep = asyncio.sleep
    delays = []

    async def sleep(delay):
        delays.append(delay)
        await real_sleep(0)

    with patch("speko_asr_python.extension.asyncio.sleep", side_effect=sleep):
        await ext._reconnect()
    return delays


@pytest.mark.asyncio
async def test_default_reconnect_is_unlimited_with_capped_backoff():
    ext, events = extension_and_events()
    ext.client = None

    async def connect():
        # The Router comes back on the eighth attempt.
        if ext.start_connection.await_count == 8:
            ext.client = MagicMock(is_ready=True)

    ext.start_connection = AsyncMock(side_effect=connect)
    delays = await _reconnect_with_recorded_delays(ext)
    assert delays == [0.5, 1, 2, 4, 4, 4, 4, 4]
    assert ext.is_connected()
    assert ext._permanent_error is None
    assert not payloads(events, "error")


@pytest.mark.asyncio
async def test_configured_reconnect_ceiling_latches_and_success_resets():
    ext, events = extension_and_events()
    ext.config = SpekoASRConfig(api_key="key", max_reconnect_attempts=2)
    ext.client = None
    ext.start_connection = AsyncMock()
    delays = await _reconnect_with_recorded_delays(ext)
    assert delays == [0.5, 1]
    assert ext.start_connection.await_count == 2
    errors = payloads(events, "error")
    assert len(errors) == 1
    assert errors[0]["code"] == int(ModuleErrorCode.FATAL_ERROR.value)
    assert errors[0]["vendor_info"]["code"] == "retry_exhausted"
    await ext._ensure_connection()
    assert ext.start_connection.await_count == 2
    ext._permanent_error = None
    ext.client = MagicMock(is_ready=True)
    ext.on_connected = AsyncMock()
    await ext._on_router_event({"type": "session.ready"})
    assert ext._reconnect_attempts == 0


@pytest.mark.asyncio
async def test_stop_cancels_commit_and_never_reports_after_shutdown():
    ext, events = extension_and_events()
    started = asyncio.Event()

    async def commit(_context=None):
        started.set()
        await asyncio.Future()

    ext.client.commit.side_effect = commit
    pending = asyncio.create_task(
        ext.on_data(ext.ten_env, finalize_data("stop", "session"))
    )
    await started.wait()
    await asyncio.wait_for(ext.on_stop(ext.ten_env), 1)
    await asyncio.wait_for(pending, 1)
    assert not payloads(events, "asr_finalize_end")
    assert ext.client is None


@pytest.mark.asyncio
@pytest.mark.parametrize("reset_clock", [False, True])
async def test_segment_timeline_two_cycles_and_reconnect(reset_clock):
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())
    await ext._on_router_event(
        {
            "type": "transcript.final",
            "text": "one",
            "segments": [{"start_ms": 0, "end_ms": 1000, "text": "one"}],
        }
    )
    await ext.on_data(ext.ten_env, finalize_data("one", "audio-session"))
    await ext._handle_audio_frame(ext.ten_env, audio())
    start = 0 if reset_clock else 1000
    await ext._on_router_event(
        {
            "type": "transcript.final",
            "text": "two",
            "segments": [
                {"start_ms": start, "end_ms": start + 1000, "text": "two"}
            ],
        }
    )

    def factory(**kwargs):
        async def connect():
            await kwargs["on_event"]({"type": "session.ready"})

        return MagicMock(
            is_ready=True,
            close=AsyncMock(),
            send_audio=AsyncMock(),
            connect=AsyncMock(side_effect=connect),
        )

    with patch(
        "speko_asr_python.extension.SpekoASRClient", side_effect=factory
    ):
        await ext.start_connection()
        await ext._handle_audio_frame(ext.ten_env, audio())
        await ext._on_router_event(
            {
                "type": "transcript.final",
                "text": "three",
                "segments": [{"start_ms": 0, "end_ms": 1000, "text": "three"}],
            }
        )
    results = payloads(events, "asr_result")
    assert [(item["start_ms"], item["duration_ms"]) for item in results] == [
        (0, 1000),
        (1000, 1000),
        (2000, 1000),
    ]
    assert [item["words"][0]["start_ms"] for item in results] == [0, 1000, 2000]
    await ext.on_stop(ext.ten_env)


@pytest.mark.asyncio
async def test_disconnected_buffer_replays_only_the_latest_session(tmp_path):
    ext, events = extension_and_events()
    ext.config.dump = True
    ext.config.dump_path = str(tmp_path / "audio.pcm")
    await ext._start_dumper()
    client = ext.client
    client.is_ready = False
    ext.auto_connect = False
    for session in ("old", "new"):
        await ext.on_audio_frame(ext.ten_env, audio({"session_id": session}))
        await ext._handle_audio_frame(
            ext.ten_env, ext.audio_frames_queue.get_nowait()
        )
    # The old session's held audio is dropped into the timeline, not merged
    # into the stream that now carries the new session.
    assert ext.buffered_frames.qsize() == 1
    assert ext._dropped_audio_bytes == 32000
    client.is_ready = True
    ext.on_connected = AsyncMock()
    await ext._on_router_event({"type": "session.ready"})
    await ext._handle_audio_frame(
        ext.ten_env, ext.audio_frames_queue.get_nowait()
    )
    assert ext.audio_frames_queue.empty()
    client.send_audio.assert_awaited_once()
    # The provider answers only after all replayed audio has gone out.
    await ext._on_router_event({"type": "transcript.final", "text": "hello"})
    assert [
        item["metadata"]["session_id"]
        for item in payloads(events, "asr_result")
    ] == ["new"]
    await ext.stop_connection()
    await ext.on_deinit(ext.ten_env)
    # The dump keeps exact ingress, including the dropped frame.
    assert (tmp_path / "audio.pcm").read_bytes() == bytes(64000)


@pytest.mark.asyncio
async def test_replay_holds_queued_audio_to_the_session_boundary():
    ext, events = extension_and_events()
    client = ext.client
    client.is_ready = False
    ext.auto_connect = False
    await ext.on_audio_frame(ext.ten_env, audio({"session_id": "old"}))
    await ext._handle_audio_frame(
        ext.ten_env, ext.audio_frames_queue.get_nowait()
    )
    assert ext.buffered_frames.qsize() == 1
    # The new session's frame is still queued when the session becomes ready.
    await ext.on_audio_frame(ext.ten_env, audio({"session_id": "new"}))
    client.is_ready = True
    ext.on_connected = AsyncMock()
    await ext._on_router_event({"type": "session.ready"})
    assert ext.audio_frames_queue.qsize() == 1
    assert ext._dropped_audio_bytes == 32000
    await ext._handle_audio_frame(
        ext.ten_env, ext.audio_frames_queue.get_nowait()
    )
    client.send_audio.assert_awaited_once()
    # The transcript arrives only after both frames were consumed.
    await ext._on_router_event({"type": "transcript.final", "text": "hello"})
    assert [
        item["metadata"]["session_id"]
        for item in payloads(events, "asr_result")
    ] == ["new"]
    await ext.stop_connection()


@pytest.mark.asyncio
async def test_disconnected_finalize_drops_the_finished_turns_held_audio():
    ext, events = extension_and_events()
    client = ext.client
    client.is_ready = False
    ext.auto_connect = False
    await ext.on_audio_frame(ext.ten_env, audio({"session_id": "turn"}))
    await ext._handle_audio_frame(
        ext.ten_env, ext.audio_frames_queue.get_nowait()
    )
    assert ext.buffered_frames.qsize() == 1
    await ext.on_data(ext.ten_env, finalize_data("done", "turn"))
    assert [
        item["finalize_id"] for item in payloads(events, "asr_finalize_end")
    ] == ["done"]
    assert ext.buffered_frames.empty()
    assert ext.buffered_frames_size == 0
    assert ext._dropped_audio_bytes == 32000
    client.is_ready = True
    ext.on_connected = AsyncMock()
    await ext._on_router_event({"type": "session.ready"})
    assert ext.audio_frames_queue.empty()
    client.send_audio.assert_not_awaited()
    client.commit.assert_not_awaited()


@pytest.mark.asyncio
async def test_dropped_audio_and_fractional_frames_have_correct_timeline():
    ext, _ = extension_and_events()
    ext.client.is_ready = False
    ext.config.buffer_duration_ms = 10
    await ext._handle_audio_frame(ext.ten_env, audio(size=640))
    assert ext.buffered_frames_size == 0
    assert ext.audio_timeline.total_dropped_audio_duration == 20
    ext.client.is_ready = True
    ext.config.sample_rate = 44100
    for _ in range(100):
        await ext._handle_audio_frame(ext.ten_env, audio(size=882))
    assert ext.audio_timeline.total_user_audio_duration == 1000
    assert ext.audio_timeline.get_audio_duration_before_time(500) == 520


@pytest.mark.asyncio
async def test_disconnected_buffer_cap_drops_oldest_frames_into_timeline():
    ext, _ = extension_and_events()
    ext.client.is_ready = False
    ext.config.sample_rate = 48000
    ext.config.channels = 8
    ext.config.buffer_duration_ms = 30000
    second = 48000 * 8 * 2  # 768000 bytes is one second at this format.
    for _ in range(14):
        await ext._handle_audio_frame(ext.ten_env, audio(size=second))
    # 14 s exceed the 10 MiB cap; the oldest second is dropped and recorded.
    assert ext.buffered_frames.qsize() == 13
    assert ext.buffered_frames_size == 13 * second <= 10 * 1024 * 1024
    assert ext.audio_timeline.total_dropped_audio_duration == 1000


@pytest.mark.parametrize(
    "params",
    [
        {"base_url": "ftp://example.com"},
        {"base_url": "https://"},
        {"base_url": "https://example.com:bad"},
        {"language": "bad tag"},
        {"ready_timeout_sec": 0},
        {"finalize_timeout_sec": float("inf")},
        {"sample_rate": 1},
        {"channels": 0},
        {"routing": {"mode": "wrong"}},
        {"buffer_duration_ms": 30001},
        {"buffer_duration_ms": -1},
        {"max_reconnect_attempts": -1},
    ],
)
def test_invalid_config_values(params):
    with pytest.raises(ValueError):
        config = SpekoASRConfig(api_key="key", params=params)
        config.update_params()


@pytest.mark.asyncio
async def test_stop_waits_for_active_send_and_closes_once():
    ext, events = extension_and_events()
    entered, release = asyncio.Event(), asyncio.Event()
    client = ext.client

    async def send(_audio):
        entered.set()
        await release.wait()

    client.send_audio.side_effect = send
    frame = audio()
    sending = asyncio.create_task(ext.send_audio(frame, "session"))
    await entered.wait()
    stopping = asyncio.create_task(ext.on_stop(ext.ten_env))
    await asyncio.sleep(0)
    assert not ext.is_connected()
    client.close.assert_not_awaited()
    release.set()
    await asyncio.wait_for(asyncio.gather(sending, stopping), 1)
    client.close.assert_awaited_once()
    frame.unlock_buf.assert_called_once()
    before = len(events)
    await ext.on_audio_frame(ext.ten_env, audio())
    assert len(events) == before


@pytest.mark.asyncio
async def test_error_and_late_disconnect_have_one_report():
    ext, events = extension_and_events()
    ext.auto_connect = False
    epoch = ext._connection_epoch
    error = SpekoRouterError("route_not_found", "no route")
    await ext._reset_connection(error, epoch)
    await ext._reset_connection(error, epoch)
    await ext.stop_connection()
    errors = payloads(events, "error")
    assert len(errors) == 1
    assert errors[0]["code"] == int(ModuleErrorCode.FATAL_ERROR.value)
    assert errors[0]["vendor_info"] == {
        "vendor": "speko",
        "code": "route_not_found",
        "message": "no route",
    }


@pytest.mark.asyncio
async def test_metrics_have_session_metadata_and_timeline_excludes_silence():
    ext, events = extension_and_events()
    await ext._handle_audio_frame(ext.ten_env, audio())
    ext.audio_timeline.add_silence_audio(500)
    await ext._handle_audio_frame(ext.ten_env, audio())

    async def commit(_context=None):
        await ext._on_router_event(
            {
                "type": "transcript.final",
                "text": "hello",
                "segments": [
                    {"text": "hello", "start_ms": 1500, "end_ms": 2500}
                ],
            }
        )

    ext.client.commit.side_effect = commit
    await ext.on_data(ext.ten_env, finalize_data("id", "audio-session"))
    await ext._on_router_event(
        {"type": "usage.updated", "usage": {"duration_ms": 2000}}
    )
    await ext._send_audio_actual_send_metrics()
    metric_messages = payloads(events, "metrics")
    metrics = {
        key: value
        for payload in metric_messages
        for key, value in payload["metrics"].items()
    }
    assert {
        "ttfw",
        "ttlw",
        "vendor_metrics",
        "actual_send",
        "actual_send_delta",
    } <= metrics.keys()
    assert all(
        payload["metadata"]["session_id"] == "audio-session"
        for payload in metric_messages
    )
    assert metrics["actual_send"] == 2500
    result = payloads(events, "asr_result")[0]
    assert (result["start_ms"], result["duration_ms"]) == (1000, 1000)


@pytest.mark.asyncio
async def test_shutdown_reports_drained_usage_without_old_callbacks():
    ext, events = extension_and_events()
    ext.metadata = {"session_id": "session"}
    ext.session_id = "session"
    client = ext.client
    client.usage = {"duration_ms": 1000}
    await ext.on_stop(ext.ten_env)
    client.close.assert_awaited_once_with(drain=True)
    metrics = payloads(events, "metrics")
    assert any(
        item["metrics"].get("vendor_metrics")
        == {"usage": {"duration_ms": 1000}}
        for item in metrics
    )


@pytest.mark.asyncio
async def test_stop_finishes_cleanup_cancelled_with_reconnect():
    ext, _ = extension_and_events()
    client = ext.client
    entered = asyncio.Event()

    async def close(*, drain):
        if not drain:
            entered.set()
            await asyncio.Future()

    client.close.side_effect = close
    await ext._reset_connection(
        SpekoRouterError("relay_error", "closed", retryable=True)
    )
    await entered.wait()
    await asyncio.wait_for(ext.on_stop(ext.ten_env), 1)
    assert client.close.await_count == 2
    assert client.close.await_args.kwargs == {"drain": True}
    assert not ext._retired_clients


@pytest.mark.asyncio
async def test_finalize_joins_handshake_and_commits_buffered_audio_before_ack():
    ext, events = extension_and_events()
    ext.client = None
    entered, ready = asyncio.Event(), asyncio.Event()
    order = []

    def factory(**kwargs):
        client = MagicMock(is_ready=False, close=AsyncMock(), usage={})

        async def connect():
            entered.set()
            await ready.wait()
            client.is_ready = True
            await kwargs["on_event"]({"type": "session.ready"})

        async def send(_audio):
            order.append(ext.session_id)

        async def commit(_context=None):
            order.append("commit")
            await kwargs["on_event"](
                {"type": "transcript.final", "text": "buffered speech"}
            )

        client.connect = connect
        client.send_audio = send
        client.commit = commit
        return client

    consumer = asyncio.create_task(ext._audio_frame_consumer())
    try:
        with patch("speko_asr_python.extension.SpekoASRClient", factory):
            connection = asyncio.create_task(ext._ensure_connection())
            await entered.wait()
            for _ in range(2):
                await ext.on_audio_frame(
                    ext.ten_env, audio({"session_id": "turn"})
                )
            await ext._audio_queue_drained.wait()
            assert ext.buffered_frames.qsize() == 2
            finalize = asyncio.create_task(
                ext.on_data(ext.ten_env, finalize_data("early", "turn"))
            )
            await asyncio.sleep(0)
            later = asyncio.create_task(
                ext.on_audio_frame(ext.ten_env, audio({"session_id": "later"}))
            )
            await asyncio.sleep(0)
            assert not payloads(events, "asr_finalize_end")
            assert not later.done()
            ready.set()
            await asyncio.wait_for(
                asyncio.gather(connection, finalize, later), 1
            )
            await ext._audio_queue_drained.wait()
        assert order == ["turn", "turn", "commit", "later"]
        assert len(payloads(events, "asr_result")) == 1
        assert payloads(events, "asr_finalize_end")[0]["finalize_id"] == "early"
    finally:
        await ext.on_stop(ext.ten_env)
        consumer.cancel()
        await asyncio.gather(consumer, return_exceptions=True)


@pytest.mark.asyncio
async def test_finalize_waits_for_already_queued_audio_on_ready_connection():
    ext, events = extension_and_events()
    order = []

    async def send(_audio):
        await asyncio.sleep(0)
        order.append(ext.session_id)

    async def commit(_context=None):
        order.append("commit")

    ext.client.send_audio.side_effect = send
    ext.client.commit.side_effect = commit
    consumer = asyncio.create_task(ext._audio_frame_consumer())
    try:
        for session in ("first", "second"):
            await ext.on_audio_frame(
                ext.ten_env, audio({"session_id": session})
            )
        await asyncio.wait_for(
            ext.on_data(ext.ten_env, finalize_data("queued", "turn")), 1
        )
        assert order == ["first", "second", "commit"]
        assert len(payloads(events, "asr_finalize_end")) == 1
    finally:
        await ext.on_stop(ext.ten_env)
        consumer.cancel()
        await asyncio.gather(consumer, return_exceptions=True)
