"""Drive known PCM through the real TTS2 base and check audio-end accounting.

Only the Router transport and the extension's clock are replaced. Requests,
flushes, frames, metrics and errors go through the base class unchanged.
"""

import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from ten_ai_base.message import ModuleErrorCode, TTSAudioEndReason
from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data

from speko_tts2_python.client import (
    SpekoRouterError,
    SpekoTTSEvent,
    SpekoTTSEventType,
)
from speko_tts2_python.config import SpekoTTS2Config
from speko_tts2_python.extension import SpekoTTS2Extension

# 24 kHz mono 16-bit PCM is 48 bytes per millisecond.
HUNDRED_MS = 4800


class FakeClock:
    """Advances only in binary-exact steps (125 ms) to keep ms arithmetic exact."""

    def __init__(self):
        self.now = 1000.0

    def monotonic(self):
        return self.now

    def time(self):
        return self.now

    def advance(self, ms):
        self.now += ms / 1000


class RecordingTenEnv:
    """Records what the real base methods emit, in emission order."""

    def __init__(self):
        self.events = []
        self.send_data = AsyncMock(side_effect=self._data)
        self.send_audio_frame = AsyncMock(side_effect=self._frame)
        for name in ("log_debug", "log_info", "log_warn", "log_error"):
            setattr(self, name, MagicMock())

    async def _data(self, data):
        raw, _ = data.get_property_to_json("")
        self.events.append((data.get_name(), json.loads(raw)))

    async def _frame(self, frame):
        buffer = frame.lock_buf()
        try:
            pcm = bytes(buffer)
        finally:
            frame.unlock_buf(buffer)
        raw, _ = frame.get_property_to_json("metadata")
        self.events.append(
            (
                "frame",
                {
                    "pcm": pcm,
                    "sample_rate": frame.get_sample_rate(),
                    "channels": frame.get_number_of_channels(),
                    "bytes_per_sample": frame.get_bytes_per_sample(),
                    "samples_per_channel": frame.get_samples_per_channel(),
                    "metadata": json.loads(raw) if raw else {},
                },
            )
        )

    def frame_sizes(self):
        return [len(frame["pcm"]) for frame in self.named("frame")]

    def pcm(self):
        return b"".join(frame["pcm"] for frame in self.named("frame"))

    def named(self, name):
        return [payload for kind, payload in self.events if kind == name]

    def index(self, predicate):
        return next(
            i for i, event in enumerate(self.events) if predicate(*event)
        )


class Script:
    """Steps for successive stream_text calls, shared by every client."""

    def __init__(self, clock, *streams):
        self.clock = clock
        self.streams = list(streams)
        self.blocked = asyncio.Event()
        self.probes = []
        self.extension = None

    def client(self, **_kwargs):
        return ScriptedClient(self)


class ScriptedClient:
    def __init__(self, script):
        self.script = script
        self.route = {"provider": "test", "model": "voice"}
        self.usage = {}
        self.request_id = "router-request"
        self.is_ready = False

    async def connect(self):
        self.is_ready = True

    async def stream_text(self, _text):
        for step, *args in self.script.streams.pop(0):
            if step == "advance":
                self.script.clock.advance(args[0])
            elif step == "ttfb":
                yield SpekoTTSEvent(SpekoTTSEventType.TTFB, args[0])
            elif step == "audio":
                yield SpekoTTSEvent(SpekoTTSEventType.AUDIO, b"\x01" * args[0])
            elif step == "pcm":
                yield SpekoTTSEvent(SpekoTTSEventType.AUDIO, args[0])
            elif step == "usage":
                self.usage = args[0]
                yield SpekoTTSEvent(SpekoTTSEventType.USAGE, args[0])
            elif step == "probe":
                self.script.probes.append(
                    self.script.extension._total_audio_bytes
                )
            elif step == "error":
                self.is_ready = False
                raise args[0]
            elif step == "block":
                self.script.blocked.set()
                await asyncio.Future()
        yield SpekoTTSEvent(SpekoTTSEventType.DONE)

    async def close(self, *, drain=True):
        del drain
        self.is_ready = False

    async def cancel(self):
        self.is_ready = False


def text_input(text, end, request_id="request", metadata=None):
    data = Data.create("tts_text_input")
    data.set_property_from_json(
        None,
        TTSTextInput(
            request_id=request_id,
            text=text,
            text_input_end=end,
            metadata=metadata or {"session_id": "session"},
        ).model_dump_json(),
    )
    return data


def pcm(size, seed):
    """Distinct bytes per stream, so mixed-up audio cannot compare equal."""
    return bytes((seed + index) % 251 for index in range(size))


def flush():
    data = Data.create("tts_flush")
    data.set_property_from_json(None, json.dumps({"flush_id": "flush"}))
    return data


async def until(predicate):
    async def poll():
        while not predicate():
            await asyncio.sleep(0.001)

    await asyncio.wait_for(poll(), 2)


class Harness:
    def __init__(self, *streams, **config):
        self.clock = FakeClock()
        self.ten_env = RecordingTenEnv()
        self.script = Script(self.clock, *streams)
        self.extension = SpekoTTS2Extension("speko")
        self.extension.config = SpekoTTS2Config(api_key="test-key", **config)
        self.extension.ten_env = self.ten_env
        self.script.extension = self.extension
        self._patches = [
            patch("speko_tts2_python.extension.time", self.clock),
            patch(
                "speko_tts2_python.extension.SpekoTTSClient",
                side_effect=self.script.client,
            ),
        ]

    async def __aenter__(self):
        for item in self._patches:
            item.start()
        self.extension.loop_task = asyncio.create_task(
            self.extension._process_input_queue(self.ten_env)
        )
        return self

    async def __aexit__(self, *_exc):
        self.extension.loop_task.cancel()
        await asyncio.gather(self.extension.loop_task, return_exceptions=True)
        for item in self._patches:
            item.stop()

    async def send(self, data):
        await self.extension.on_data(self.ten_env, data)

    def ends(self):
        return self.ten_env.named("tts_audio_end")

    def idle(self):
        return (
            self.extension._processing_request_id is None
            and len(self.extension.input_queue) == 0
        )


def is_ttfb(kind, payload):
    return kind == "metrics" and "ttfb" in payload["metrics"]


def is_usage(kind, payload):
    return kind == "metrics" and "total_recv_audio_duration" in (
        payload["metrics"]
    )


@pytest.mark.asyncio
async def test_success_order_duration_and_interval():
    async with Harness(
        [
            ("ttfb", 120),
            ("audio", HUNDRED_MS),
            ("advance", 125),
            ("audio", HUNDRED_MS),
            ("advance", 125),
            ("usage", {"characters": 5}),
        ]
    ) as harness:
        await harness.send(text_input("hello", True))
        await until(lambda: harness.ends() and harness.idle())
    env = harness.ten_env
    start = env.index(lambda kind, _: kind == "tts_audio_start")
    ttfb = env.index(is_ttfb)
    first_frame = env.index(lambda kind, _: kind == "frame")
    end = env.index(lambda kind, _: kind == "tts_audio_end")
    usage = env.index(is_usage)
    assert start < ttfb < first_frame < end < usage
    assert env.frame_sizes() == [HUNDRED_MS, HUNDRED_MS]
    assert env.events[ttfb][1]["metrics"]["ttfb"] == 120
    (payload,) = harness.ends()
    assert payload["reason"] == TTSAudioEndReason.REQUEST_END
    assert payload["request_total_audio_duration_ms"] == 200
    # Measured from tts_audio_start, not from the first audio frame.
    assert payload["request_event_interval_ms"] == 250
    assert payload["metadata"]["router_usage"] == {"characters": 5}
    assert not env.named("error")


@pytest.mark.asyncio
async def test_partial_audio_then_final_error_ends_once_before_error():
    async with Harness(
        [
            ("ttfb", 80),
            ("audio", HUNDRED_MS),
            ("advance", 125),
            (
                "error",
                SpekoRouterError("provider_error", "boom", retryable=True),
            ),
        ]
    ) as harness:
        await harness.send(text_input("hello", True))
        await until(lambda: harness.ten_env.named("error") and harness.idle())
    env = harness.ten_env
    (payload,) = harness.ends()
    assert payload["reason"] == TTSAudioEndReason.ERROR
    assert payload["request_total_audio_duration_ms"] == 100
    assert payload["request_event_interval_ms"] == 125
    (error,) = env.named("error")
    assert error["code"] == int(ModuleErrorCode.NON_FATAL_ERROR.value)
    assert error["vendor_info"] == {
        "vendor": "speko",
        "code": "provider_error",
        "message": "boom",
    }
    end = env.index(lambda kind, _: kind == "tts_audio_end")
    assert end < env.index(lambda kind, _: kind == "error")


@pytest.mark.asyncio
async def test_non_final_error_keeps_request_open_until_one_end():
    async with Harness(
        [
            ("ttfb", 80),
            ("audio", HUNDRED_MS),
            ("advance", 125),
            (
                "error",
                SpekoRouterError("provider_error", "boom", retryable=True),
            ),
        ],
        [("audio", HUNDRED_MS), ("advance", 125)],
    ) as harness:
        await harness.send(text_input("first part", False))
        await until(lambda: harness.ten_env.named("error"))
        assert not harness.ends()
        await harness.send(text_input("second part", True))
        await until(lambda: harness.ends() and harness.idle())
    env = harness.ten_env
    (payload,) = harness.ends()
    assert payload["reason"] == TTSAudioEndReason.REQUEST_END
    assert payload["request_total_audio_duration_ms"] == 200
    assert payload["request_event_interval_ms"] == 250
    (error,) = env.named("error")
    assert error["vendor_info"]["vendor"] == "speko"
    assert env.index(lambda kind, _: kind == "error") < env.index(
        lambda kind, _: kind == "tts_audio_end"
    )
    assert len(env.named("tts_audio_start")) == 1


@pytest.mark.asyncio
async def test_flush_mid_stream_ends_once_as_interrupted():
    async with Harness(
        [
            ("ttfb", 60),
            ("audio", HUNDRED_MS),
            ("advance", 125),
            ("block",),
        ]
    ) as harness:
        await harness.send(text_input("hello", True))
        await until(harness.script.blocked.is_set)
        await harness.send(flush())
        await until(lambda: harness.ten_env.named("tts_flush_end"))
        # Give a late completion of the cancelled stream a chance to emit.
        for _ in range(20):
            await asyncio.sleep(0.001)
    env = harness.ten_env
    (payload,) = harness.ends()
    assert payload["reason"] == TTSAudioEndReason.INTERRUPTED
    assert payload["request_total_audio_duration_ms"] == 100
    assert payload["request_event_interval_ms"] == 125
    assert env.index(lambda kind, _: kind == "tts_audio_end") < env.index(
        lambda kind, _: kind == "tts_flush_end"
    )
    assert not env.named("error")


@pytest.mark.asyncio
async def test_no_audio_completion_ends_once_with_zero_duration():
    async with Harness([("advance", 125)]) as harness:
        await harness.send(text_input("hello", True))
        await until(lambda: harness.ends() and harness.idle())
    env = harness.ten_env
    (payload,) = harness.ends()
    assert payload["reason"] == TTSAudioEndReason.REQUEST_END
    assert payload["request_total_audio_duration_ms"] == 0
    assert payload["request_event_interval_ms"] == 0
    assert not env.named("frame")
    assert len(env.named("tts_audio_start")) == 1


@pytest.mark.asyncio
async def test_odd_sized_chunks_count_only_emitted_frames():
    async with Harness(
        [
            ("ttfb", 50),
            ("audio", HUNDRED_MS + 1),
            ("probe",),
            ("audio", HUNDRED_MS - 1),
            ("probe",),
        ]
    ) as harness:
        await harness.send(text_input("hello", True))
        await until(lambda: harness.ends() and harness.idle())
    # The odd byte waits in the base's leftover buffer for the next chunk.
    assert harness.script.probes == [HUNDRED_MS, 2 * HUNDRED_MS]
    assert harness.ten_env.frame_sizes() == [HUNDRED_MS, HUNDRED_MS]
    (payload,) = harness.ends()
    assert payload["request_total_audio_duration_ms"] == 200


@pytest.mark.asyncio
@pytest.mark.parametrize("sample_rate,channels", [(24000, 1), (16000, 2)])
async def test_emitted_pcm_and_frame_format_follow_the_output_config(
    sample_rate, channels
):
    frame_bytes = 2 * channels
    tenth = sample_rate * frame_bytes // 10
    audio = pcm(2 * tenth, seed=7)
    # The first chunk ends one byte into a sample frame.
    split = tenth + 1
    async with Harness(
        [("ttfb", 50), ("pcm", audio[:split]), ("pcm", audio[split:])],
        sample_rate=sample_rate,
        channels=channels,
    ) as harness:
        await harness.send(text_input("hello", True))
        await until(lambda: harness.ends() and harness.idle())
    env = harness.ten_env
    assert env.pcm() == audio
    for frame in env.named("frame"):
        assert (
            frame["sample_rate"],
            frame["channels"],
            frame["bytes_per_sample"],
        ) == (sample_rate, channels, 2)
        assert frame["samples_per_channel"] * frame_bytes == len(frame["pcm"])
    (payload,) = harness.ends()
    assert payload["request_total_audio_duration_ms"] == 200


@pytest.mark.asyncio
async def test_back_to_back_requests_keep_their_own_audio_and_end():
    first, second = pcm(HUNDRED_MS, seed=1), pcm(2 * HUNDRED_MS, seed=2)
    async with Harness(
        [("ttfb", 40), ("pcm", first[:1001]), ("pcm", first[1001:])],
        [("ttfb", 60), ("pcm", second)],
    ) as harness:
        # The second request is queued while the first one streams.
        await harness.send(text_input("one", True, "r1", {"session_id": "s1"}))
        await harness.send(text_input("two", True, "r2", {"session_id": "s2"}))
        await until(lambda: len(harness.ends()) == 2 and harness.idle())
    audio = {"r1": [], "r2": []}
    current = None
    for kind, payload in harness.ten_env.events:
        if kind == "tts_audio_start":
            assert current is None
            current = payload["request_id"]
        elif kind == "frame":
            audio[current].append(payload)
        elif kind == "tts_audio_end":
            assert payload["request_id"] == current
            current = None
    assert b"".join(frame["pcm"] for frame in audio["r1"]) == first
    assert b"".join(frame["pcm"] for frame in audio["r2"]) == second
    for request, session in (("r1", "s1"), ("r2", "s2")):
        assert {
            frame["metadata"].get("session_id") for frame in audio[request]
        } == {session}
    ends = harness.ends()
    assert [payload["request_id"] for payload in ends] == ["r1", "r2"]
    assert [payload["request_total_audio_duration_ms"] for payload in ends] == [
        100,
        200,
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize("terminal", ["completion", "flush", "error"])
async def test_dump_holds_exactly_the_emitted_pcm(tmp_path, terminal):
    # The odd last byte stays in the base's leftover and is never emitted.
    audio = pcm(HUNDRED_MS + 1, seed=3)
    tail = {
        "completion": [],
        "flush": [("block",)],
        "error": [
            (
                "error",
                SpekoRouterError("provider_error", "boom", retryable=True),
            )
        ],
    }[terminal]
    async with Harness(
        [("ttfb", 30), ("pcm", audio), *tail],
        dump=True,
        dump_path=str(tmp_path),
    ) as harness:
        await harness.send(text_input("hello", True))
        if terminal == "flush":
            await until(harness.script.blocked.is_set)
            await harness.send(flush())
            await until(lambda: harness.ten_env.named("tts_flush_end"))
        else:
            await until(lambda: harness.ends() and harness.idle())
    emitted = harness.ten_env.pcm()
    assert emitted == audio[:-1]
    assert (tmp_path / "speko_tts_request.pcm").read_bytes() == emitted
