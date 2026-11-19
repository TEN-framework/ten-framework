#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Extension-level request lifecycle and audio-end accounting tests.

Deterministic: the vendor synthesis is replaced with fixed PCM, so emitted
bytes are known exactly. These tests drive the extension through TEN
input/output and assert tts_audio_start, tts_audio_end and error events,
including the mandatory accounting fields:
request_total_audio_duration_ms == emitted_bytes * 1000 /
(sample_rate * sample_width * channels), and one end event per request.
"""
import io
import json
import struct
import tempfile
import threading
import wave
from datetime import datetime as real_datetime, timedelta

import pytest
from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from gmicloud_tts2_python.client import GmicloudTTSApiError, GmicloudTTSClient

# ExtensionTester drives the real TEN runtime; without it (bare unit-test
# environments) these tests are skipped, like the framework's own
# extension-level suites.
_REAL_TEN_RUNTIME = getattr(ExtensionTester, "__name__", "") == "ExtensionTester"
pytestmark = pytest.mark.skipif(
    not _REAL_TEN_RUNTIME,
    reason="requires the real TEN runtime (run via the tts_guarder environment)",
)

SAMPLE_RATE = 24000


def make_wav(ms: int) -> bytes:
    samples = SAMPLE_RATE * ms // 1000
    pcm = struct.pack(f"<{samples}h", *([0] * samples))
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm)
    return buf.getvalue()


def base_config(**overrides) -> dict:
    config = {
        "dump": False,
        "dump_path": "/tmp",
        "params": {
            "api_key": "test-key-value",
            "base_url": "https://console.gmicloud.ai",
            "model": "inworld-tts-2",
            "voice": "Dennis",
            "speed": 1.0,
            "audio_format": "WAV",
            "sample_rate": SAMPLE_RATE,
            "channels": 1,
            "text_chunk_chars": 2000,
            "request_timeout_sec": 5.0,
            "poll_interval_sec": 0.01,
            "poll_attempts": 3,
        },
    }
    config["params"].update(overrides)
    return config


class LifecycleTester(ExtensionTester):
    def __init__(self, requests):
        super().__init__()
        self.requests = requests  # list of (request_id, text)
        self.audio_bytes = 0
        self.starts = []
        self.ends = []
        self.errors = []
        self.metrics_events = []

    def on_start(self, ten_env: TenEnvTester) -> None:
        for request_id, text in self.requests:
            data = Data.create("tts_text_input")
            data.set_property_from_json(
                None,
                TTSTextInput(
                    request_id=request_id,
                    text=text,
                    text_input_end=True,
                ).model_dump_json(),
            )
            ten_env.send_data(data)
        ten_env.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str)
        if name == "tts_audio_start":
            self.starts.append(payload)
        elif name == "tts_audio_end":
            self.ends.append(payload)
            if len(self.ends) >= len(self.requests):
                ten_env.stop_test()
        elif name == "metrics":
            self.metrics_events.append(payload)
        elif name == "error":
            self.errors.append(payload)

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame) -> None:
        buf = audio_frame.lock_buf()
        try:
            self.audio_bytes += len(buf)
        finally:
            audio_frame.unlock_buf(buf)


def run_tester(tester, config) -> None:
    tester.set_test_mode_single("gmicloud_tts2_python", json.dumps(config))
    error = tester.run()
    assert error is None, f"tester failed: {error}"


@pytest.fixture
def patch_synth(monkeypatch):
    def apply(side_effects):
        calls = []

        async def fake(self, text, request_id):
            calls.append(text)
            effect = side_effects[min(len(calls) - 1, len(side_effects) - 1)]
            if isinstance(effect, Exception):
                raise effect
            return effect

        monkeypatch.setattr(GmicloudTTSClient, "_synthesize_part", fake)
        return calls

    return apply


class FlushRecoveryTester(ExtensionTester):
    """Sends one request, flushes after the first audio frame, then sends a
    second request once flush_end arrives (recovery on the same extension)."""

    def __init__(self, dump_path):
        super().__init__()
        self.dump_path = dump_path
        self.audio_bytes = {}
        self.ends = []
        self.errors = []
        self.flush_end_seen = False
        self.frame_gate = threading.Event()

    def on_start(self, ten_env: TenEnvTester) -> None:
        data = Data.create("tts_text_input")
        data.set_property_from_json(
            None,
            TTSTextInput(
                request_id="req-flush-a", text="One. Two.", text_input_end=True
            ).model_dump_json(),
        )
        ten_env.send_data(data)
        ten_env.on_start_done()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame) -> None:
        buf = audio_frame.lock_buf()
        try:
            key = "req-flush-a" if not self.flush_end_seen else "req-flush-b"
            self.audio_bytes[key] = self.audio_bytes.get(key, 0) + len(buf)
        finally:
            audio_frame.unlock_buf(buf)
        if not self.frame_gate.is_set():
            self.frame_gate.set()
            flush = Data.create("tts_flush")
            flush.set_property_string("flush_id", "flush-1")
            flush.set_property_from_json(
                "metadata", json.dumps({"session_id": "flush-session"})
            )
            ten_env.send_data(flush)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}
        if name == "error":
            self.errors.append(payload)
        elif name == "tts_flush_end":
            self.flush_end_seen = True
            data = Data.create("tts_text_input")
            data.set_property_from_json(
                None,
                TTSTextInput(
                    request_id="req-flush-b", text="Hi.", text_input_end=True
                ).model_dump_json(),
            )
            ten_env.send_data(data)
        elif name == "tts_audio_end":
            self.ends.append(payload)
            if len(self.ends) >= 2:
                ten_env.stop_test()


def test_success_accounting(patch_synth):
    patch_synth([make_wav(1000)])
    tester = LifecycleTester([("req-1", "hello world")])
    run_tester(tester, base_config())

    assert len(tester.starts) == 1 and tester.starts[0]["request_id"] == "req-1"
    assert len(tester.ends) == 1, "exactly one tts_audio_end expected"
    end = tester.ends[0]
    assert end["request_id"] == "req-1"
    # 48000 emitted bytes at 24kHz mono s16 == exactly 1000 ms
    assert end["request_total_audio_duration_ms"] == 1000
    # interval is wall-clock from audio start to end; bound it sanely
    assert 0 <= end["request_event_interval_ms"] < 5000
    assert tester.audio_bytes == 48000
    assert tester.errors == []


def test_partial_audio_then_error(patch_synth):
    patch_synth([
        make_wav(500),
        GmicloudTTSApiError("vendor failed"),
    ])
    tester = LifecycleTester([("req-err", "One. Two.")])
    run_tester(tester, base_config(text_chunk_chars=6))

    assert tester.audio_bytes == 24000, "only the first part's PCM is emitted"
    assert len(tester.errors) == 1 and tester.errors[0]["code"] == 1000
    assert len(tester.ends) == 1, "one end event even after a partial error"
    assert tester.ends[0]["request_total_audio_duration_ms"] == 500


def test_no_audio_termination(patch_synth):
    patch_synth([make_wav(1000)])
    tester = LifecycleTester([("req-empty", "   ")])
    run_tester(tester, base_config())

    assert tester.audio_bytes == 0
    assert tester.errors == []
    assert len(tester.ends) <= 1, "at most one end event for empty input"
    for end in tester.ends:
        assert end["request_total_audio_duration_ms"] == 0


def test_flush_after_audio_then_recovery(patch_synth, monkeypatch):
    import asyncio

    gate = threading.Event()
    calls = []

    async def gated_synth(self, text, request_id):
        calls.append((request_id, text))
        if len(calls) == 1:
            return make_wav(500)  # A part 1: flows immediately
        if len(calls) == 2:
            # A part 2: stays in flight until the flush cancels it
            await asyncio.sleep(2.0)
            return make_wav(250)
        return make_wav(250)  # B

    monkeypatch.setattr(GmicloudTTSClient, "_synthesize_part", gated_synth)
    dump_path = tempfile.mkdtemp(prefix="gmicloud-lifecycle-")
    tester = FlushRecoveryTester(dump_path=dump_path)
    config = base_config(text_chunk_chars=6)  # A = "One. Two." -> two parts
    config["dump"] = True
    config["dump_path"] = dump_path
    run_tester(tester, config)

    by_id = {end["request_id"]: end for end in tester.ends}
    assert len(tester.ends) == 2, f"one end per request, got: {tester.ends}"
    # the interrupted request keeps only the emitted part's audio
    assert by_id["req-flush-a"]["reason"] == 2  # TTSAudioEndReason.INTERRUPTED
    assert by_id["req-flush-a"]["request_total_audio_duration_ms"] == 500
    # the second request recovers through the same extension instance
    assert by_id["req-flush-b"]["reason"] == 1  # TTSAudioEndReason.REQUEST_END
    assert by_id["req-flush-b"]["request_total_audio_duration_ms"] == 250
    assert tester.errors == []
    assert tester.audio_bytes.get("req-flush-a", 0) == 24000
    assert tester.audio_bytes.get("req-flush-b", 0) == 12000
    # dump files are finalized for both requests
    import os
    for request_id in ("req-flush-a", "req-flush-b"):
        path = os.path.join(dump_path, f"gmicloud_dump_{request_id}.pcm")
        assert os.path.exists(path) and os.path.getsize(path) > 0


def test_exact_interval_controlled_clock(patch_synth, monkeypatch):
    patch_synth([make_wav(1000)])
    t0 = real_datetime(2026, 1, 1)

    class ScriptedDateTime:
        # call 1 stamps input arrival, call 2 stamps audio start (the
        # interval origin), the final call stamps the end measurement
        steps = [10_000, 0, 200, 437]
        index = 0

        @classmethod
        def now(cls):
            offset = cls.steps[min(cls.index, len(cls.steps) - 1)]
            cls.index += 1
            return t0 + timedelta(milliseconds=offset)

    monkeypatch.setattr("ten_ai_base.tts2_http.datetime", ScriptedDateTime)
    tester = LifecycleTester([("req-interval", "hello world")])
    run_tester(tester, base_config())

    assert len(tester.ends) == 1
    assert tester.ends[0]["request_event_interval_ms"] == 437


def test_request_metrics(patch_synth):
    patch_synth([make_wav(1000)])
    tester = LifecycleTester([("req-metrics", "hello world")])
    run_tester(tester, base_config())

    ttfb_metrics = [
        m for m in tester.metrics_events if "ttfb" in m.get("metrics", {})
    ]
    assert ttfb_metrics, "a TTFB request metric must be emitted"
    assert all(m.get("module") == "tts" for m in ttfb_metrics)
    assert all(m.get("vendor") == "gmicloud" for m in ttfb_metrics)


def test_recovery_after_error(patch_synth):
    patch_synth([
        GmicloudTTSApiError("vendor failed"),
        make_wav(250),
    ])
    tester = LifecycleTester([("req-a", "First."), ("req-b", "Second.")])
    run_tester(tester, base_config())

    assert len(tester.errors) == 1 and tester.errors[0]["code"] == 1000
    assert len(tester.ends) == 2, "each request gets exactly one end event"
    by_id = {end["request_id"]: end for end in tester.ends}
    assert by_id["req-b"]["request_total_audio_duration_ms"] == 250
    assert tester.audio_bytes == 12000
