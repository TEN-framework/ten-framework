"""
Basic functionality tests for Gemini TTS extension
"""

import asyncio
import base64
import datetime as dt
import json
from types import SimpleNamespace

import pytest
from ten_ai_base.struct import TTS2HttpResponseEventType

from gemini_tts_python.config import GeminiTTSConfig
from gemini_tts_python.gemini_tts import GeminiTTSClient


def test_import():
    """Test that extension can be imported"""
    from gemini_tts_python.extension import GeminiTTSExtension

    assert GeminiTTSExtension is not None


def test_config_flash_model():
    """Test configuration model with Flash model"""
    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-tts",
            "voice": "Kore",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-3.8-flash-tts"
    assert config.params["voice"] == "Kore"


def test_config_lite_model():
    """Test configuration model with Flash Lite model"""
    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-lite-tts",
            "voice": "Charon",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-3.8-flash-lite-tts"
    assert config.params["voice"] == "Charon"


def test_default_model_uses_current_tts_model():
    config = GeminiTTSConfig(params={"api_key": "test_key"})
    config.update_params()

    assert config.params["model"] == "gemini-3.8-flash-tts"


def test_client_requests_raw_pcm_from_interactions():
    pcm = b"\x00\x01\x02\x03"

    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

        def log_info(self, *_args, **_kwargs):
            pass

    class FakeInteractions:
        def __init__(self):
            self.payload = None

        async def create(self, **kwargs):
            self.payload = kwargs
            return SimpleNamespace(
                output_audio=SimpleNamespace(
                    data=base64.b64encode(pcm).decode("ascii")
                )
            )

    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-tts",
            "voice": "Kore",
            "language_code": "en-US",
            "style_prompt": "Speak calmly",
        }
    )
    client = GeminiTTSClient(config, FakeTenEnv())
    interactions = FakeInteractions()
    client.client = SimpleNamespace(
        aio=SimpleNamespace(interactions=interactions)
    )

    events = asyncio.run(_collect_events(client))

    assert interactions.payload["model"] == "gemini-3.8-flash-tts"
    text_content = interactions.payload["input"][0]["content"][0]
    assert text_content["text"] == "Hello"
    assert text_content["annotations"] == [
        {"type": "speech_metadata", "style": "Speak calmly"}
    ]
    assert interactions.payload["response_format"] == {
        "type": "audio",
        "mime_type": "audio/l16",
        "sample_rate": 24000,
    }
    assert interactions.payload["generation_config"]["speech_config"] == [
        {"voice": "Kore", "language": "en-US"}
    ]
    assert events == [
        (pcm, TTS2HttpResponseEventType.RESPONSE),
        (None, TTS2HttpResponseEventType.END),
    ]


async def _collect_events(client):
    return [event async for event in client.get("Hello", "request-1")]


def test_client_reports_missing_audio():
    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

    config = GeminiTTSConfig(params={"api_key": "test_key"})
    client = GeminiTTSClient(config, FakeTenEnv())

    async def create(**_kwargs):
        return SimpleNamespace(output_audio=None)

    client.client = SimpleNamespace(
        aio=SimpleNamespace(interactions=SimpleNamespace(create=create))
    )

    events = asyncio.run(_collect_events(client))

    assert events[0][1] == TTS2HttpResponseEventType.ERROR
    assert b"No audio data" in events[0][0]


def test_client_classifies_authentication_error():
    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

    config = GeminiTTSConfig(params={"api_key": "bad_key"})
    client = GeminiTTSClient(config, FakeTenEnv())

    async def create(**_kwargs):
        raise RuntimeError("401 unauthorized")

    client.client = SimpleNamespace(
        aio=SimpleNamespace(interactions=SimpleNamespace(create=create))
    )

    events = asyncio.run(_collect_events(client))

    assert events[0][1] == TTS2HttpResponseEventType.INVALID_KEY_ERROR


def test_cancel_during_request_and_recovery():
    pcm = b"\x00\x01\x02\x03"
    started = asyncio.Event()
    release = asyncio.Event()
    calls = 0

    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

        def log_info(self, *_args, **_kwargs):
            pass

    class FakeInteractions:
        async def create(self, **_kwargs):
            nonlocal calls
            calls += 1
            if calls == 1:
                started.set()
                await release.wait()
            return SimpleNamespace(
                output_audio=SimpleNamespace(
                    data=base64.b64encode(pcm).decode("ascii")
                )
            )

    config = GeminiTTSConfig(params={"api_key": "test_key"})
    client = GeminiTTSClient(config, FakeTenEnv())
    client.client = SimpleNamespace(
        aio=SimpleNamespace(interactions=FakeInteractions())
    )

    async def scenario():
        first = asyncio.create_task(_collect_events(client))
        await asyncio.wait_for(started.wait(), 1)
        await client.cancel()
        release.set()
        assert await first == []
        return await _collect_events(client)

    assert asyncio.run(scenario()) == [
        (pcm, TTS2HttpResponseEventType.RESPONSE),
        (None, TTS2HttpResponseEventType.END),
    ]


def test_client_handles_sequential_requests():
    pcm = b"\x10\x20\x30\x40"
    calls = 0

    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

    class FakeInteractions:
        async def create(self, **_kwargs):
            nonlocal calls
            calls += 1
            return SimpleNamespace(
                output_audio=SimpleNamespace(
                    data=base64.b64encode(pcm + bytes([calls])).decode("ascii")
                )
            )

    config = GeminiTTSConfig(params={"api_key": "test_key"})
    client = GeminiTTSClient(config, FakeTenEnv())
    client.client = SimpleNamespace(
        aio=SimpleNamespace(interactions=FakeInteractions())
    )

    async def scenario():
        first = await _collect_events(client)
        second = await _collect_events(client)
        return first, second

    first, second = asyncio.run(scenario())
    assert first[0][1] == TTS2HttpResponseEventType.RESPONSE
    assert second[0][1] == TTS2HttpResponseEventType.RESPONSE
    assert first[0][0] == pcm + b"\x01"
    assert second[0][0] == pcm + b"\x02"


def test_extension_emits_sequential_audio_end_accounting(monkeypatch):
    runtime = pytest.importorskip("ten_runtime")
    if not hasattr(runtime, "ExtensionTester"):
        pytest.skip("TEN runtime test harness is not installed")
    from ten_ai_base import tts2_http
    from ten_ai_base.struct import TTSTextInput
    from ten_runtime import Data, ExtensionTester

    pcm = b"\x00\x01" * 2400
    clock_ms = 0

    class LifecycleTester(ExtensionTester):
        def __init__(self):
            super().__init__()
            self.starts = []
            self.ends = []
            self.received_audio_bytes = 0

        def send_request(self, ten_env, request_id):
            tts_input = TTSTextInput(
                request_id=request_id,
                text="Hello",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env.send_data(data)

        def on_start(self, ten_env_tester):
            self.send_request(ten_env_tester, "first")
            ten_env_tester.on_start_done()

        def on_audio_frame(self, _ten_env, audio_frame):
            buf = audio_frame.lock_buf()
            try:
                self.received_audio_bytes += len(buf)
            finally:
                audio_frame.unlock_buf(buf)

        def on_data(self, ten_env, data):
            name = data.get_name()
            if name == "tts_audio_start":
                payload, _ = data.get_property_to_json(None)
                self.starts.append(json.loads(payload)["request_id"])
                return
            if name != "tts_audio_end":
                return
            payload, _ = data.get_property_to_json(None)
            self.ends.append(json.loads(payload))
            if len(self.ends) == 1:
                self.send_request(ten_env, "second")
            else:
                ten_env.stop_test()

    async def fake_get(_self, _text, request_id):
        nonlocal clock_ms
        clock_ms += 10
        yield pcm, TTS2HttpResponseEventType.RESPONSE
        assert request_id in {"first", "second"}
        clock_ms += 20
        yield None, TTS2HttpResponseEventType.END

    real_datetime = tts2_http.datetime

    class ControlledDateTime:
        @classmethod
        def now(cls):
            return real_datetime.now() + dt.timedelta(milliseconds=clock_ms)

    monkeypatch.setattr(GeminiTTSClient, "get", fake_get)
    monkeypatch.setattr(tts2_http, "datetime", ControlledDateTime)
    tester = LifecycleTester()
    tester.set_test_mode_single(
        "gemini_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test-key",
                    "model": "gemini-3.8-flash-tts",
                    "voice": "Kore",
                    "language_code": "en-US",
                }
            }
        ),
    )

    tester.run()

    assert tester.starts == ["first", "second"]
    assert len(tester.ends) == 2
    assert tester.received_audio_bytes == len(pcm) * 2
    for end in tester.ends:
        assert end["reason"] == 1
        assert end["request_total_audio_duration_ms"] == 100
        assert end["request_event_interval_ms"] == 20
