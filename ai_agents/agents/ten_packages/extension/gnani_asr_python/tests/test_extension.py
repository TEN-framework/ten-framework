import asyncio
import json
import os
from pathlib import Path

import pytest
from ten_runtime import AsyncExtensionTester, AudioFrame, Data, TenError, TenErrorCode
from ten_packages.extension.gnani_asr_python.config import PrismaConfig
from ten_packages.extension.gnani_asr_python import extension


class Socket:
    def __init__(self):
        self.messages = asyncio.Queue()
        self.audio = bytearray()
        self.turn = 0

    async def recv(self):
        return '{"type":"connected"}'

    def __aiter__(self):
        return self

    async def __anext__(self):
        return json.dumps(await self.messages.get())

    async def close(self):
        pass

    async def send(self, data):
        if isinstance(data, bytes):
            assert len(data) == 1024
            self.audio.extend(data)
        elif json.loads(data)["type"] == "commit":
            for n in range(2):
                await self.messages.put(
                    dict(type="speech_start", utterance_id=f"{self.turn}-{n}", t_ms=0)
                )
            await asyncio.sleep(0.05)
            for n in range(2):
                await self.messages.put(
                    dict(
                        type="transcript",
                        utterance_id=f"{self.turn}-{n}",
                        text="hello world",
                        audio_duration_ms=16,
                    )
                )
            self.turn += 1


class RecognitionTester(AsyncExtensionTester):
    def __init__(self, audio):
        super().__init__()
        self.audio = audio
        self.finished = asyncio.Event()
        self.events = []

    async def on_start(self, env):
        self.sender = asyncio.create_task(self.stream(env))

    async def stream(self, env):
        try:
            for turn in range(2):
                self.finished.clear()
                for i in range(0, len(self.audio), 320):
                    chunk = self.audio[i : i + 320]
                    frame = AudioFrame.create("pcm_frame")
                    frame.set_sample_rate(16000)
                    frame.set_number_of_channels(1)
                    frame.set_bytes_per_sample(2)
                    frame.set_property_from_json("metadata", '{"session_id":"example"}')
                    frame.alloc_buf(len(chunk))
                    buf = frame.lock_buf()
                    buf[:] = chunk
                    frame.unlock_buf(buf)
                    await env.send_audio_frame(frame)
                data = Data.create("asr_finalize")
                data.set_property_string("finalize_id", str(turn))
                await env.send_data(data)
                await asyncio.wait_for(self.finished.wait(), 20)
            env.stop_test()
        except Exception as exc:
            env.stop_test(
                TenError.create(TenErrorCode.ErrorCodeGeneric, type(exc).__name__)
            )

    async def on_data(self, env, data):
        name = data.get_name()
        if name in ("error", "asr_result", "asr_finalize_end"):
            raw, _ = data.get_property_to_json(None)
            self.events.append((name, json.loads(raw)))
            if name == "asr_finalize_end":
                self.finished.set()

    async def on_stop(self, env):
        self.sender.cancel()
        await asyncio.gather(self.sender, return_exceptions=True)


def test_queued_finalize(monkeypatch):
    socket = Socket()

    async def connect(url, additional_headers, **kwargs):
        assert additional_headers["model"] == "prisma-v2.5"
        return socket

    monkeypatch.setattr(extension, "connect", connect)
    tester = RecognitionTester(bytes(range(256)) * 5)
    tester.set_test_mode_single(
        "gnani_asr_python", json.dumps({"params": {"api_key": "test"}})
    )
    assert tester.run() is None
    assert socket.audio == (tester.audio + bytes(768)) * 2
    assert [name for name, _ in tester.events] == [
        "asr_result",
        "asr_result",
        "asr_finalize_end",
    ] * 2
    assert [
        e["finalize_id"] for name, e in tester.events if name == "asr_finalize_end"
    ] == ["0", "1"]


@pytest.mark.skipif(
    not os.environ.get("GNANI_API_KEY"), reason="requires live Prisma credential"
)
@pytest.mark.parametrize(
    "model,language",
    [("prisma-v2.5", "en-IN"), ("prisma-v3.0", "en-IN"), ("prisma-v3.0", "indic-auto")],
)
def test_live(model, language):
    fixture = (
        Path(__file__).resolve().parents[4]
        / "integration_tests/asr_guarder/tests/test_data/16k_en_us_helloworld.pcm"
    )
    tester = RecognitionTester(fixture.read_bytes())
    tester.set_test_mode_single(
        "gnani_asr_python",
        json.dumps(
            {
                "params": {
                    "model": model,
                    "language": language,
                    "api_key": "${env:GNANI_API_KEY}",
                }
            }
        ),
    )
    assert tester.run() is None
    assert [n for n, _ in tester.events] == ["asr_result", "asr_finalize_end"] * 2
    for name, result in tester.events:
        if name == "asr_result":
            assert "hello world" in result["text"].lower()
            assert result["language"] == "en-IN"
            assert result["metadata"]["session_id"] == "example"
            assert result["metadata"]["asr_info"]["vendor"] == "gnani"


@pytest.mark.parametrize(
    "options",
    [
        {"model": "unknown"},
        {"language": "hi-IN,en-IN"},
        {"language": "indic-auto"},
        {"sample_rate": 8000},
    ],
)
def test_invalid_config(options):
    with pytest.raises(ValueError):
        PrismaConfig(api_key="secret", **options)


@pytest.mark.asyncio
async def test_acknowledges_each_silent_request(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    ext.finalize_id = "silent"
    ext.last_finalize_time = 1
    completed = AsyncMock()
    monkeypatch.setattr(ext, "send_asr_finalize_end", completed)
    await ext.finalize(None)
    await ext.finalize(None)
    assert completed.await_count == 2
    assert ext.last_finalize_time is None


@pytest.mark.asyncio
async def test_reconnect_preserves_only_unsent_bytes(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    first = AsyncMock()
    first.send.side_effect = OSError("transport closed")
    ext.ws = first
    ext.ready.set()
    ext.tail.extend(b"a" * 1024 + b"b" * 1024)
    with pytest.raises(OSError):
        await ext.packet(1024)
    second = AsyncMock()
    ext.ws = second
    await ext.packet(1024)
    second.send.assert_awaited_once_with(b"b" * 1024)
    assert not ext.tail


@pytest.mark.asyncio
async def test_disconnect_retires_pending_finalize(monkeypatch):
    from unittest.mock import AsyncMock, Mock

    ext = extension.GnaniASRExtension("gnani")
    socket = AsyncMock()
    ext.ws = socket
    ext.finalize_id = "first"
    ext.dirty = True
    ext.starts["unfinished"] = 0
    completed = AsyncMock()
    failed = AsyncMock()
    monkeypatch.setattr(ext, "send_asr_finalize_end", completed)
    monkeypatch.setattr(ext, "schedule_retry", Mock())
    monkeypatch.setattr(ext, "error", failed)
    task = asyncio.create_task(ext.finalize(None))
    while socket.send.await_count < 1:
        await asyncio.sleep(0)
    ext.ws = None
    ext.changed.set()
    await asyncio.wait_for(task, 1)
    completed.assert_awaited_once()
    failed.assert_awaited_once()
    assert not ext.dirty


@pytest.mark.asyncio
async def test_overflow_keeps_finalize_and_newest_audio(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    monkeypatch.setattr(ext, "error", AsyncMock())

    def frame():
        audio = AudioFrame.create("pcm_frame")
        audio.alloc_buf(96000)
        return audio

    old, new = frame(), frame()
    barrier = Data.create("asr_finalize")
    await ext.on_audio_frame(None, old)
    await ext.on_data(None, barrier)
    await ext.on_audio_frame(None, new)
    assert list(ext.inputs) == [extension.DroppedAudio(96000), barrier, new]
    assert ext.queued_bytes == 96000
    assert ext.audio_timeline.total_dropped_audio_duration == 0


@pytest.mark.asyncio
async def test_transport_reconnect_and_shutdown(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    ext.config = PrismaConfig(api_key="local")
    ext.ten_env = AsyncMock()
    for name in (
        "on_connected",
        "on_disconnected",
        "send_connect_delay_metrics",
        "error",
        "_emit_connection_transition",
    ):
        monkeypatch.setattr(ext, name, AsyncMock())

    class Dropped(Socket):
        async def __anext__(self):
            ext.processing = 1
            raise StopAsyncIteration

    fake = AsyncMock(side_effect=[Dropped(), Socket()])
    monkeypatch.setattr(extension, "connect", fake)
    await ext.start_connection()
    await ext.receiver
    await asyncio.wait_for(ext.retry, 2)
    assert ext.is_connected()
    assert fake.await_count == 2
    assert ext.processing == 0
    receiver = ext.receiver
    ext.stopped = True
    await ext.stop_connection()
    assert receiver.done()
    assert not ext.is_connected()


@pytest.mark.skipif(
    not os.environ.get("GNANI_DESIGNER_GRAPH") or not os.environ.get("GNANI_API_KEY"),
    reason="requires a saved Designer graph and live Prisma credential",
)
def test_saved_designer_graph():
    graphs = json.loads(Path(os.environ["GNANI_DESIGNER_GRAPH"]).read_text())["ten"][
        "predefined_graphs"
    ]
    graph = next(g["graph"] for g in graphs if g["name"] == "voice_assistant")
    stt = next(n for n in graph["nodes"] if n["name"] == "stt")
    assert stt["addon"] == "gnani_asr_python"
    fixture = (
        Path(__file__).resolve().parents[4]
        / "integration_tests/asr_guarder/tests/test_data/16k_en_us_helloworld.pcm"
    )
    tester = RecognitionTester(fixture.read_bytes())
    tester.set_test_mode_single(stt["addon"], json.dumps(stt["property"]))
    assert tester.run() is None
    assert [name for name, _ in tester.events] == ["asr_result", "asr_finalize_end"] * 2
    for name, payload in tester.events:
        if name == "asr_result":
            assert "hello world" in payload["text"].lower()
            assert payload["language"] == "en-IN"


@pytest.mark.asyncio
async def test_disconnected_audio_before_finalize(monkeypatch):
    from unittest.mock import AsyncMock, Mock

    ext = extension.GnaniASRExtension("gnani")
    ext.ten_env = Mock()
    completed = asyncio.Event()

    async def acknowledge():
        completed.set()

    monkeypatch.setattr(ext, "send_asr_finalize_end", acknowledge)
    monkeypatch.setattr(ext, "error", AsyncMock())
    f = AudioFrame.create("pcm_frame")
    f.alloc_buf(320)
    await ext.on_audio_frame(ext.ten_env, f)
    req = Data.create("asr_finalize")
    req.set_property_string("finalize_id", "offline")
    await ext.on_data(ext.ten_env, req)
    worker = asyncio.create_task(ext._audio_frame_consumer())
    try:
        await asyncio.wait_for(completed.wait(), 7)
        assert ext.finalize_id == "offline"
        assert ext.queued_bytes == 0
    finally:
        worker.cancel()
        await asyncio.gather(worker, return_exceptions=True)


@pytest.mark.asyncio
async def test_stop_invalidates_inflight_handshake(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    ext.config = PrismaConfig(api_key="local")
    ext.ten_env = AsyncMock()
    socket = Socket()
    started = asyncio.Event()

    async def handshake():
        started.set()
        await asyncio.Event().wait()

    socket.recv = handshake
    socket.close = AsyncMock()
    monkeypatch.setattr(extension, "connect", AsyncMock(return_value=socket))
    monkeypatch.setattr(ext, "_emit_connection_transition", AsyncMock())
    attempt = asyncio.create_task(ext.start_connection())
    await asyncio.wait_for(started.wait(), 1)
    await ext.stop_connection()
    await asyncio.wait_for(attempt, 1)
    socket.close.assert_awaited_once()
    assert not ext.is_connected()


@pytest.mark.asyncio
async def test_settings_reply_cannot_overtake_late_transcript(monkeypatch):
    from unittest.mock import AsyncMock, Mock

    ext = extension.GnaniASRExtension("gnani")
    ext.config = PrismaConfig(api_key="local")
    ext.ws = socket = Socket()
    ext.dirty = True
    ext.audio_timeline.add_user_audio(32)
    ext.finalize_id = "delayed"
    ext.ten_env = Mock()
    monkeypatch.setattr(extension, "DRAIN_SECONDS", 1.5)
    order = []

    async def result(value):
        order.append(value.text)

    async def end():
        order.append("end")

    monkeypatch.setattr(ext, "send_asr_result", result)
    monkeypatch.setattr(ext, "send_asr_finalize_end", end)
    monkeypatch.setattr(ext, "schedule_retry", Mock())
    monkeypatch.setattr(ext, "on_disconnected", AsyncMock())
    socket.send = AsyncMock()
    ext.receiver = asyncio.create_task(ext.receive(socket))
    pending = asyncio.create_task(ext.finalize(None))
    await socket.messages.put(
        {
            "type": "transcript",
            "segment_id": "zero",
            "audio_duration_ms": 16,
            "text": "early result",
        }
    )
    await socket.messages.put({"type": "settings_updated"})
    await asyncio.sleep(0.65)
    assert not pending.done()
    await socket.messages.put({"type": "processing"})
    await socket.messages.put(
        {
            "type": "transcript",
            "segment_id": "one",
            "audio_duration_ms": 32,
            "text": "delayed result",
        }
    )
    await asyncio.wait_for(pending, 2)
    assert order == ["early result", "delayed result", "end"]
    await socket.messages.put(
        {
            "type": "transcript",
            "segment_id": "late",
            "audio_duration_ms": 16,
            "text": "stale",
        }
    )
    assert ext.receiver.done()
    ext.ten_env.log_warn.assert_called_once()
    assert not ext.is_connected()


@pytest.mark.asyncio
async def test_transient_retries_have_capped_delay(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    delays = []

    async def sleep(delay):
        delays.append(delay)

    async def attempt():
        if len(delays) == 7:
            ext.ws = Socket()

    monkeypatch.setattr(extension.asyncio, "sleep", sleep)
    monkeypatch.setattr(ext, "_ensure_connection", attempt)
    monkeypatch.setattr(ext, "error", AsyncMock())
    await ext.reconnect()
    assert delays == [0.25, 0.5, 1, 2, 4, 4, 4]
    assert not ext.fatal


@pytest.mark.asyncio
async def test_bad_credentials_redacted_and_terminal(monkeypatch):
    from types import SimpleNamespace
    from unittest.mock import AsyncMock
    from websockets.exceptions import InvalidStatus

    ext = extension.GnaniASRExtension("gnani")
    ext.config = PrismaConfig(api_key="local-secret")
    exc = InvalidStatus(SimpleNamespace(status_code=401))
    monkeypatch.setattr(extension, "connect", AsyncMock(side_effect=exc))
    errors = AsyncMock()
    monkeypatch.setattr(ext, "send_asr_error", errors)
    monkeypatch.setattr(ext, "on_disconnected", AsyncMock())
    monkeypatch.setattr(ext, "_emit_connection_transition", AsyncMock())
    await ext.start_connection()
    assert ext.fatal and ext.retry is None
    _, vendor = errors.await_args.args
    assert vendor.code == "401"
    assert "local-secret" not in vendor.message


def test_invalid_key_configuration_disconnects():
    class StateTester(AsyncExtensionTester):
        def __init__(self):
            super().__init__()
            self.states = []

        async def on_start(self, env):
            self.timer = asyncio.get_running_loop().call_later(8, env.stop_test)

        async def on_data(self, env, data):
            if data.get_name() == "connection_status_changed":
                raw, _ = data.get_property_to_json(None)
                state = json.loads(raw)["current"]
                self.states.append(state)
                if state == "disconnected":
                    env.stop_test()

        async def on_stop(self, env):
            self.timer.cancel()

    tester = StateTester()
    tester.set_test_mode_single("gnani_asr_python", '{"params":{"api_key":""}}')
    assert tester.run() is None
    assert tester.states[-2:] == ["connecting", "disconnected"]


@pytest.mark.asyncio
async def test_normal_close_at_commit_preserves_arrival_time(monkeypatch):
    from unittest.mock import AsyncMock, Mock
    from websockets.exceptions import ConnectionClosedOK
    from websockets.frames import Close

    ext = extension.GnaniASRExtension("gnani")
    ext.finalize_arrival = asyncio.get_running_loop().time() - 2
    ext.dirty = True
    ext.ws = Socket()

    async def close_at_send(_):
        assert asyncio.get_running_loop().time() - ext.last_finalize_time >= 2
        raise ConnectionClosedOK(Close(1000, ""), Close(1000, ""), True)

    ext.ws.send = close_at_send
    monkeypatch.setattr(ext, "schedule_retry", Mock())
    monkeypatch.setattr(ext, "on_disconnected", AsyncMock())
    monkeypatch.setattr(ext, "error", AsyncMock())
    monkeypatch.setattr(ext, "send_asr_finalize_end", AsyncMock())
    await ext.finalize(None)
    ext.error.assert_not_awaited()
    ext.on_disconnected.assert_awaited_once()
    ext.send_asr_finalize_end.assert_awaited_once()
    assert ext.last_finalize_time is None
