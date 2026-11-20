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
    def __init__(self, sample_rate):
        self.sample_rate = sample_rate
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
        ) == (self.sample_rate, 2, 1)
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
async def adapter(profile, tmp_path, transport, sample_rate=16000):
    model, voice = profile
    rec = Recorder(sample_rate)
    ext = GnaniTTSExtension("gnani")
    ext.ten_env = rec.env
    ext.config = GnaniTTSConfig(
        params=dict(
            api_key="unit-test", model=model, voice=voice, sample_rate=sample_rate
        ),
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
                yield b'event: error\ndata: {"message":"Unsupported voice: private provider detail"}\n\n'
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
        if scenario == "sse_error":
            assert errors[0]["message"] == "Gnani synthesis error: unsupported voice"

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


@pytest.mark.asyncio
@pytest.mark.parametrize("rate", [8000, 16000, 22050, 24000, 44100, 48000])
async def test_split_pcm_at_configured_rate(profile, rate, tmp_path, clock):
    model, voice = profile
    text = "Hello. " * 200
    pcm = bytes(range(256)) * 32
    fragments = [pcm[:1]] + [pcm[i : i + 207] for i in range(1, len(pcm), 207)]

    class Stream(httpx.AsyncByteStream):
        async def __aiter__(self):
            for fragment in fragments:
                clock.ms += 7
                yield audio_record(fragment)
            clock.ms += 12
            yield COMPLETE

    def handle(request):
        assert request.headers["X-API-Key-ID"] == "unit-test"
        assert json.loads(request.content) == {
            "text": text,
            "voice": voice,
            "language": "en-IN",
            "model": model,
            "audio_config": {
                "container": "raw",
                "encoding": "linear_pcm",
                "sample_rate": rate,
                "num_channels": 1,
                "sample_width": 2,
            },
        }
        return httpx.Response(200, stream=Stream())

    async with adapter(profile, tmp_path, handle, rate) as (_, rec, submit, _send):
        await submit("next", text)
        await asyncio.wait_for(rec.finished.wait(), 2)

    assert rec.starts() == ["next"] and not rec.bodies("error")
    assert all(owner == "next" and len(frame) % 2 == 0 for owner, frame in rec.frames)
    assert b"".join(frame for _, frame in rec.frames) == pcm
    assert (tmp_path / "gnani_dump_next.pcm").read_bytes() == pcm
    assert rec.ttfb() == [("next", 7)]
    # The first playable sample arrives in the second fragment.
    assert rec.ends() == [
        (
            "next",
            TTSAudioEndReason.REQUEST_END,
            len(pcm) * 1000 // (2 * rate),
            (len(fragments) - 2) * 7 + 12,
        )
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize("outcome", ["retry", "exhausted", "flush"])
async def test_rate_limit_recovery(profile, outcome, tmp_path, monkeypatch, clock):
    attempts, closed, delays = [], [], []
    in_backoff = asyncio.Event()
    sleep = asyncio.sleep

    async def wait(delay, result=None):
        if delay != 1:
            return await sleep(delay, result)
        delays.append(delay)
        in_backoff.set()
        if outcome == "flush":
            await asyncio.Event().wait()
        clock.ms += 1000

    monkeypatch.setattr(asyncio, "sleep", wait)
    pcm = b"\x01\x02" * 160

    class Stream(httpx.AsyncByteStream):
        def __init__(self, key):
            self.key = key

        async def __aiter__(self):
            clock.ms += 15
            yield audio_record(pcm)
            clock.ms += 5
            yield COMPLETE

        async def aclose(self):
            closed.append(self.key)

    def handle(request):
        assert closed == attempts
        owner = request.extensions["ten_request_id"]
        attempt = attempts.count(owner) + 1
        attempts.append(owner)
        if owner == "first" and (outcome != "retry" or attempt == 1):
            return httpx.Response(
                429, headers={"Retry-After": "1"}, stream=Stream(owner)
            )
        return httpx.Response(200, stream=Stream(owner))

    async with adapter(profile, tmp_path, handle) as (_, rec, submit, send):
        await submit("first")
        if outcome == "flush":
            await asyncio.wait_for(in_backoff.wait(), 2)
            await send("tts_flush", flush_id="backoff")
        await submit("next")
        await asyncio.wait_for(rec.finished.wait(), 2)

    retries = {"retry": 1, "exhausted": 3, "flush": 1}[outcome]
    assert len(delays) == retries
    assert (
        attempts
        == closed
        == ["first"] * {"retry": 2, "exhausted": 4, "flush": 1}[outcome] + ["next"]
    )
    expected_audio = ["first", "next"] if outcome == "retry" else ["next"]
    assert rec.starts() == expected_audio
    assert rec.frames == [(owner, pcm) for owner in expected_audio]
    expected_ends, expected_ttfb = [], []
    if outcome == "retry":
        expected_ends.append(("first", TTSAudioEndReason.REQUEST_END, 10, 5))
        expected_ttfb.append(("first", 1015))
    elif outcome == "exhausted":
        expected_ends.append(("first", TTSAudioEndReason.ERROR, 0, 3000))
    assert rec.ends() == expected_ends + [
        ("next", TTSAudioEndReason.REQUEST_END, 10, 5)
    ]
    assert rec.ttfb() == expected_ttfb + [("next", 15)]
    errors = [
        (body["id"], body["code"], body["message"]) for body in rec.bodies("error")
    ]
    assert errors == (
        [("first", int(ModuleErrorCode.NON_FATAL_ERROR), "Gnani HTTP 429")]
        if outcome == "exhausted"
        else []
    )
    assert [body["flush_id"] for body in rec.bodies("tts_flush_end")] == (
        ["backoff"] if outcome == "flush" else []
    )
