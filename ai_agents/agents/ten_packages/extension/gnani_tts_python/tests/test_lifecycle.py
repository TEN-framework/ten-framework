import asyncio
import base64
import json
from contextlib import asynccontextmanager
from datetime import datetime, timedelta
from unittest.mock import Mock

import httpx
import pytest
from ten_ai_base import tts2_http
from ten_ai_base.message import ModuleErrorCode, TTSAudioEndReason
from ten_runtime import Data

from ten_packages.extension.gnani_tts_python import gnani_tts
from ten_packages.extension.gnani_tts_python.config import GnaniTTSConfig
from ten_packages.extension.gnani_tts_python.extension import GnaniTTSExtension


@pytest.fixture(
    params=[("timbre-v2.5", "Nalini"), ("timbre-v3.0", "Shivani")], ids=["v2.5", "v3.0"]
)
def profile(request):
    return request.param


@pytest.fixture
def clock(monkeypatch):
    class Clock:
        ms = 0

        def now(self):
            return datetime(2025, 1, 1) + timedelta(milliseconds=self.ms)

        def monotonic_ns(self):
            return self.ms * 1_000_000

    clock = Clock()
    monkeypatch.setattr(tts2_http, "datetime", clock)
    monkeypatch.setattr(gnani_tts, "time", clock)
    return clock


def audio_record(pcm):
    return b'event: chunk\ndata: {"audio":"' + base64.b64encode(pcm) + b'"}\n\n'


COMPLETE = b"event: complete\ndata: {}\n\n"


class Recorder:
    def __init__(self):
        self.events = []
        self.frames = []
        self.finished = asyncio.Event()
        self.env = Mock(send_data=self.on_data, send_audio_frame=self.on_frame)

    async def on_data(self, data):
        body = json.loads(data.get_property_to_json("")[0])
        name = data.get_name()
        self.events.append((name, body))
        if name == "tts_audio_end" and body["request_id"] == "next":
            self.finished.set()

    async def on_frame(self, frame):
        turn = json.loads(frame.get_property_to_json("metadata")[0])["turn"]
        assert (
            frame.get_sample_rate(),
            frame.get_bytes_per_sample(),
            frame.get_number_of_channels(),
        ) == (16000, 2, 1)
        self.frames.append((turn, bytes(frame.get_buf())))

    def bodies(self, event):
        return [body for name, body in self.events if name == event]

    def starts(self):
        return [body["request_id"] for body in self.bodies("tts_audio_start")]

    def ends(self):
        return [
            (
                body["request_id"],
                body["reason"],
                body["request_total_audio_duration_ms"],
                body["request_event_interval_ms"],
            )
            for body in self.bodies("tts_audio_end")
        ]

    def ttfb(self):
        return [
            (body["metadata"]["turn"], body["metrics"]["ttfb"])
            for body in self.bodies("metrics")
            if "ttfb" in body["metrics"]
        ]


@asynccontextmanager
async def adapter(profile, tmp_path, transport):
    model, voice = profile
    rec = Recorder()
    ext = GnaniTTSExtension("gnani")
    ext.ten_env = rec.env
    ext.config = GnaniTTSConfig(
        params=dict(api_key="unit-test", model=model, voice=voice, sample_rate=16000),
        dump=True,
        dump_path=str(tmp_path),
    )
    ext.client = await ext.create_client(ext.config, rec.env)
    await ext.client.http.aclose()
    ext.client.http = httpx.AsyncClient(transport=httpx.MockTransport(transport))
    worker = asyncio.create_task(ext._process_input_queue(rec.env))

    async def send(name, **properties):
        data = Data.create(name)
        data.set_property_from_json(None, json.dumps(properties))
        await ext.on_data(rec.env, data)

    async def submit(request_id, text="Hello", end=True):
        await send(
            "tts_text_input",
            request_id=request_id,
            text=text,
            text_input_end=end,
            metadata={"turn": request_id},
        )

    try:
        yield ext, rec, submit, send
    finally:
        await ext.input_queue.put(None)
        try:
            await asyncio.wait_for(worker, 2)
        finally:
            await ext.client.clean()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "scenario",
    [
        "before_audio",
        "during_audio",
        "closing",
        "sse_error",
        "auth",
        "timeout",
        "network",
        "truncated",
        "partial_truncated",
        "wrong_content",
    ],
)
async def test_stream_lifecycle_and_next_request(profile, scenario, clock, tmp_path):
    waiting = asyncio.Event()
    release_close = asyncio.Event()
    closed = []
    first_pcm = b"\x01\x02" * 1600
    next_pcm = b"\x03\x04" * 800
    has_audio = scenario in {
        "during_audio",
        "closing",
        "sse_error",
        "partial_truncated",
    }
    interrupt = scenario in {"before_audio", "during_audio", "closing"}

    class Stream(httpx.AsyncByteStream):
        def __init__(self, request_id):
            self.request_id = request_id

        async def __aiter__(self):
            first = self.request_id == "first"
            if not first or has_audio:
                clock.ms += 30
                yield audio_record(first_pcm if first else next_pcm)
            clock.ms += 250
            if first and scenario in {"before_audio", "during_audio"}:
                waiting.set()
                await asyncio.Event().wait()
            if not first or scenario == "closing":
                yield COMPLETE
            elif scenario == "sse_error":
                yield b"event: error\ndata: {}\n\n"
            elif scenario == "wrong_content":
                yield b"<html>upstream error</html>"

        async def aclose(self):
            if self.request_id == "first" and scenario == "closing":
                waiting.set()
                await release_close.wait()
            closed.append(self.request_id)

    def handle(request):
        assert json.loads(request.content)["model"] == profile[0]
        request_id = request.extensions["ten_request_id"]
        if request_id == "first":
            if scenario == "timeout":
                raise httpx.ReadTimeout("private provider detail", request=request)
            if scenario == "network":
                raise httpx.ConnectError("private provider detail", request=request)
            if scenario == "auth":
                return httpx.Response(401, stream=Stream(request_id))
        mime = (
            "text/html"
            if request_id == "first" and scenario == "wrong_content"
            else "text/event-stream"
        )
        return httpx.Response(
            200, headers={"Content-Type": mime}, stream=Stream(request_id)
        )

    async with adapter(profile, tmp_path, handle) as (ext, rec, submit, send):
        await submit("first")
        if interrupt:
            await asyncio.wait_for(waiting.wait(), 2)
            flush = asyncio.create_task(send("tts_flush", flush_id="interrupt"))
            # Let flush reach the active provider task before releasing its close.
            await asyncio.sleep(0)
            release_close.set()
            await asyncio.wait_for(flush, 2)
            assert closed == ["first"]
            assert ext.client.response is None and not ext.recorder_map
        await submit("next")
        await asyncio.wait_for(rec.finished.wait(), 2)

    errors = rec.bodies("error")
    if interrupt:
        assert not errors
        assert [body["flush_id"] for body in rec.bodies("tts_flush_end")] == [
            "interrupt"
        ]
    else:
        assert not rec.bodies("tts_flush_end")
        assert len(errors) == 1
        assert (
            errors[0]["id"] == "first" and errors[0]["vendor_info"]["vendor"] == "gnani"
        )
        assert errors[0]["code"] == int(
            ModuleErrorCode.FATAL_ERROR
            if scenario == "auth"
            else ModuleErrorCode.NON_FATAL_ERROR
        )
        assert "private provider detail" not in errors[0]["message"]

    assert closed == (
        ["next"] if scenario in {"timeout", "network"} else ["first", "next"]
    )
    assert rec.starts() == (["first"] if has_audio else []) + ["next"]
    expected_ends = []
    if has_audio or not interrupt:
        reason = TTSAudioEndReason.INTERRUPTED if interrupt else TTSAudioEndReason.ERROR
        # Before audio, the base reports time since the request was sent.
        interval = 250 if has_audio or scenario in {"truncated", "wrong_content"} else 0
        expected_ends.append(("first", reason, 100 if has_audio else 0, interval))
    assert rec.ends() == expected_ends + [
        ("next", TTSAudioEndReason.REQUEST_END, 50, 250)
    ]
    assert rec.ttfb() == ([("first", 30)] if has_audio else []) + [("next", 30)]
    assert rec.frames == ([("first", first_pcm)] if has_audio else []) + [
        ("next", next_pcm)
    ]
    first_dump = tmp_path / "gnani_dump_first.pcm"
    if has_audio:
        assert first_dump.read_bytes() == first_pcm
    else:
        assert not first_dump.exists() or first_dump.stat().st_size == 0
    assert (tmp_path / "gnani_dump_next.pcm").read_bytes() == next_pcm


@pytest.mark.asyncio
async def test_appended_and_interleaved_requests(profile, clock, tmp_path):
    # Request B is queued, including its final segment, while A is still in flight.
    started, resume = asyncio.Event(), asyncio.Event()
    calls = []
    pcm = {
        "A1": b"\x01\x00" * 160,
        "A2": b"\x02\x00" * 320,
        "B1": b"\x03\x00" * 480,
        "B2": b"\x04\x00" * 640,
    }

    class Stream(httpx.AsyncByteStream):
        def __init__(self, text):
            self.text = text

        async def __aiter__(self):
            if self.text == "A1":
                started.set()
                await resume.wait()
            clock.ms += 10 if self.text.startswith("A") else 20
            yield audio_record(pcm[self.text])
            clock.ms += 50
            yield COMPLETE

    def handle(request):
        text = json.loads(request.content)["text"]
        calls.append((request.extensions["ten_request_id"], text))
        return httpx.Response(200, stream=Stream(text))

    async with adapter(profile, tmp_path, handle) as (_, rec, submit, _send):
        await submit("first", "A1", end=False)
        await asyncio.wait_for(started.wait(), 2)
        await submit("next", "B1", end=False)
        await submit("next", "B2")
        await submit("first", "A2")
        resume.set()
        await asyncio.wait_for(rec.finished.wait(), 2)

    assert calls == [("first", "A1"), ("first", "A2"), ("next", "B1"), ("next", "B2")]
    assert rec.frames == [(owner, pcm[text]) for owner, text in calls]
    assert rec.starts() == ["first", "next"]
    assert rec.ends() == [
        ("first", TTSAudioEndReason.REQUEST_END, 30, 110),
        ("next", TTSAudioEndReason.REQUEST_END, 70, 120),
    ]
    assert rec.ttfb() == [("first", 10), ("next", 20)]
    assert not rec.bodies("error")
    for owner, texts in [("first", ("A1", "A2")), ("next", ("B1", "B2"))]:
        assert (tmp_path / f"gnani_dump_{owner}.pcm").read_bytes() == b"".join(
            pcm[t] for t in texts
        )
