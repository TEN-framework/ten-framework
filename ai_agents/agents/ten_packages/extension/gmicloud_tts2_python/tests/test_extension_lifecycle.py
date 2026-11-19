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
import wave

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
