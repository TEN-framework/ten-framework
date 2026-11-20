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
        else:
            await self.messages.put(dict(type="settings_updated"))
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
async def test_finalizes_silence_once(monkeypatch):
    from unittest.mock import AsyncMock

    ext = extension.GnaniASRExtension("gnani")
    ext.finalize_id = "silent"
    completed = AsyncMock()
    monkeypatch.setattr(ext, "send_asr_finalize_end", completed)
    await ext.finalize(None)
    await ext.finalize(None)
    completed.assert_awaited_once()


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
    while socket.send.await_count < 2:
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
            raise StopAsyncIteration

    fake = AsyncMock(side_effect=[Dropped(), Socket()])
    monkeypatch.setattr(extension, "connect", fake)
    await ext.start_connection()
    await ext.receiver
    await asyncio.wait_for(ext.retry, 2)
    assert ext.is_connected()
    assert fake.await_count == 2
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
