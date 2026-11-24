"""
Basic functionality tests for Gemini TTS extension
"""

import asyncio
import base64
import datetime as dt
import json
from importlib import import_module
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest
from ten_ai_base.struct import TTS2HttpResponseEventType

from gemini_tts_python.config import GeminiTTSConfig
from gemini_tts_python.gemini_tts import GeminiTTSClient


class FakeAsyncStream:
    def __init__(self, events):
        self.events = iter(events)
        self.closed = False

    def __aiter__(self):
        return self

    async def __anext__(self):
        try:
            return next(self.events)
        except StopIteration as exc:
            raise StopAsyncIteration from exc

    async def close(self):
        self.closed = True


def audio_event(data):
    return SimpleNamespace(
        event_type="step.delta",
        delta=SimpleNamespace(type="audio", data=data),
    )


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
            return FakeAsyncStream(
                [
                    audio_event(base64.b64encode(pcm).decode("ascii")),
                ]
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
    assert interactions.payload["store"] is False
    assert interactions.payload["stream"] is True
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
        return FakeAsyncStream([])

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
            return FakeAsyncStream(
                [
                    audio_event(base64.b64encode(pcm).decode("ascii")),
                ]
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
            return FakeAsyncStream(
                [
                    audio_event(
                        base64.b64encode(pcm + bytes([calls])).decode("ascii")
                    ),
                ]
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


def test_clean_closes_async_sdk_client():
    class FakeTenEnv:
        def log_info(self, *_args, **_kwargs):
            pass

    client = GeminiTTSClient(
        GeminiTTSConfig(params={"api_key": "test_key"}),
        FakeTenEnv(),
    )
    close = AsyncMock()
    client.client = SimpleNamespace(aio=SimpleNamespace(aclose=close))

    asyncio.run(client.clean())

    close.assert_awaited_once()
    assert client.client is None


@pytest.mark.parametrize(
    "blocked",
    [True, False],
    ids=["before-audio", "after-audio"],
)
def test_extension_flush_cancels_audio_and_recovers(
    monkeypatch,
    tmp_path,
    blocked,
):
    runtime = pytest.importorskip("ten_runtime")
    if not hasattr(runtime, "ExtensionTester"):
        pytest.skip("TEN runtime test harness is not installed")
    from ten_ai_base import tts2_http
    from ten_ai_base.struct import TTSFlush, TTSTextInput
    from ten_runtime import Data, ExtensionTester

    gemini_tts_module = import_module(
        "ten_packages.extension.gemini_tts_python.gemini_tts"
    )

    pcm = b"\x00\x01" * 2400
    clock_ms = 0
    provider_calls = 0
    provider_cancelled = False
    sent_audio_requests = []

    class FlushTester(ExtensionTester):
        def __init__(self):
            super().__init__()
            self.starts = []
            self.ends = []
            self.flush_ends = []
            self.errors = []
            self.received_audio_bytes = 0

        def send_request(self, ten_env, request_id):
            request = TTSTextInput(
                request_id=request_id,
                text="Hello",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, request.model_dump_json())
            ten_env.send_data(data)

        def on_start(self, ten_env_tester):
            self.ten_env = ten_env_tester
            self.send_request(ten_env_tester, "first")
            ten_env_tester.on_start_done()

        def on_audio_frame(self, ten_env, audio_frame):
            buf = audio_frame.lock_buf()
            try:
                self.received_audio_bytes += len(buf)
            finally:
                audio_frame.unlock_buf(buf)

            if not blocked and len(sent_audio_requests) == 1:
                flush = Data.create("tts_flush")
                flush.set_property_from_json(
                    None,
                    TTSFlush(flush_id="flush-first").model_dump_json(),
                )
                ten_env.send_data(flush)

        def on_data(self, ten_env, data):
            name = data.get_name()
            if name == "error":
                payload, _ = data.get_property_to_json(None)
                self.errors.append(json.loads(payload))
                return

            payload, _ = data.get_property_to_json(None)
            event = json.loads(payload) if payload else {}
            if name == "tts_audio_start":
                self.starts.append(event["request_id"])
            elif name == "tts_audio_end":
                self.ends.append(event)
                if event["request_id"] == "second":
                    ten_env.stop_test()
            elif name == "tts_flush_end":
                self.flush_ends.append(event)
                self.send_request(ten_env, "second")

    tester = FlushTester()

    class FakeAsyncInteractions:
        async def create(self, **_kwargs):
            nonlocal clock_ms, provider_calls, provider_cancelled
            provider_calls += 1
            clock_ms += 10
            if blocked and provider_calls == 1:
                flush = Data.create("tts_flush")
                flush.set_property_from_json(
                    None,
                    TTSFlush(flush_id="flush-first").model_dump_json(),
                )
                tester.ten_env.send_data(flush)
                try:
                    await asyncio.Event().wait()
                except asyncio.CancelledError:
                    provider_cancelled = True
                    raise
            return FakeAsyncStream(
                [
                    audio_event(base64.b64encode(pcm).decode("ascii")),
                ]
            )

    class FakeClient:
        def __init__(self, **_kwargs):
            self.aio = SimpleNamespace(
                interactions=FakeAsyncInteractions(),
                aclose=AsyncMock(),
            )

    original_send_audio_data = (
        tts2_http.AsyncTTS2HttpExtension.send_tts_audio_data
    )

    async def send_audio_data(extension, audio_data):
        nonlocal clock_ms
        request_id = extension.current_request_id
        sent_audio_requests.append(request_id)
        clock_ms += 20
        await original_send_audio_data(extension, audio_data)
        if not blocked and request_id == "first":
            await asyncio.sleep(1)

    base_time = dt.datetime(2026, 1, 1)

    class ControlledDateTime:
        @classmethod
        def now(cls):
            return base_time + dt.timedelta(milliseconds=clock_ms)

    monkeypatch.setattr(
        gemini_tts_module,
        "genai",
        SimpleNamespace(Client=FakeClient),
    )
    monkeypatch.setattr(
        tts2_http.AsyncTTS2HttpExtension,
        "send_tts_audio_data",
        send_audio_data,
    )
    monkeypatch.setattr(tts2_http, "datetime", ControlledDateTime)
    tester.set_test_mode_single(
        "gemini_tts_python",
        json.dumps(
            {
                "dump": True,
                "dump_path": str(tmp_path),
                "params": {
                    "api_key": "test-key",
                    "model": "gemini-3.8-flash-tts",
                },
            }
        ),
    )

    tester.run()

    assert tester.errors == []
    assert provider_cancelled is blocked
    assert len(tester.flush_ends) == 1
    if blocked:
        assert tester.starts == ["second"]
        assert sent_audio_requests == ["second"]
        assert len(tester.ends) == 1
        assert tester.ends[0]["request_id"] != "first"
        assert tester.ends[0]["request_id"] == "second"
        assert tester.ends[0]["reason"] == 1
        assert tester.ends[0]["request_total_audio_duration_ms"] == 100
        assert tester.received_audio_bytes == len(pcm)
    else:
        assert tester.starts == ["first", "second"]
        assert sent_audio_requests == ["first", "second"]
        assert len(tester.ends) == 2
        assert tester.ends[0]["request_id"] == "first"
        assert tester.ends[0]["reason"] == 2
        assert tester.ends[0]["request_total_audio_duration_ms"] == 100
        assert tester.ends[0]["request_event_interval_ms"] == 20
        assert tester.ends[1]["request_id"] == "second"
        assert tester.ends[1]["reason"] == 1
        assert tester.ends[1]["request_total_audio_duration_ms"] == 100
        assert tester.received_audio_bytes == len(pcm) * 2
    assert (tmp_path / "gemini_dump_first.pcm").read_bytes() == (
        b"" if blocked else pcm
    )
    assert (tmp_path / "gemini_dump_second.pcm").read_bytes() == pcm
    assert provider_calls == 2


def test_extension_interleaved_requests_keep_metrics(monkeypatch):
    runtime = pytest.importorskip("ten_runtime")
    if not hasattr(runtime, "ExtensionTester"):
        pytest.skip("TEN runtime test harness is not installed")
    from ten_ai_base import tts2_http
    from ten_ai_base.struct import TTSTextInput
    from ten_runtime import Data, ExtensionTester

    gemini_tts_module = import_module(
        "ten_packages.extension.gemini_tts_python.gemini_tts"
    )

    pcm = b"\x00\x01" * 2400
    clock_ms = 0
    request_texts = []

    class InterleavedTester(ExtensionTester):
        def __init__(self):
            super().__init__()
            self.starts = []
            self.ends = []
            self.ttfb_turns = []

        def send_request(
            self,
            ten_env,
            request_id,
            text,
            text_input_end,
            turn_id,
        ):
            request = TTSTextInput(
                request_id=request_id,
                text=text,
                text_input_end=text_input_end,
                metadata={"turn_id": turn_id},
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, request.model_dump_json())
            ten_env.send_data(data)

        def on_start(self, ten_env_tester):
            self.send_request(ten_env_tester, "first", "Hello", False, 1)
            self.send_request(ten_env_tester, "second", "Second", True, 2)
            self.send_request(ten_env_tester, "first", ", world", True, 1)
            ten_env_tester.on_start_done()

        def on_data(self, ten_env, data):
            payload, _ = data.get_property_to_json(None)
            event = json.loads(payload) if payload else {}
            if data.get_name() == "tts_audio_start":
                self.starts.append(event["request_id"])
            elif data.get_name() == "tts_audio_end":
                self.ends.append(event)
                if len(self.ends) == 2:
                    ten_env.stop_test()
            elif data.get_name() == "metrics" and "ttfb" in event.get(
                "metrics", {}
            ):
                self.ttfb_turns.append(event["metadata"]["turn_id"])

    class FakeAsyncInteractions:
        async def create(self, **kwargs):
            nonlocal clock_ms
            clock_ms += 10
            request_texts.append(kwargs["input"][0]["content"][0]["text"])
            return FakeAsyncStream(
                [
                    audio_event(base64.b64encode(pcm).decode("ascii")),
                ]
            )

    class FakeClient:
        def __init__(self, **_kwargs):
            self.aio = SimpleNamespace(
                interactions=FakeAsyncInteractions(),
                aclose=AsyncMock(),
            )

    original_send_audio_data = (
        tts2_http.AsyncTTS2HttpExtension.send_tts_audio_data
    )

    async def send_audio_data(extension, audio_data):
        nonlocal clock_ms
        clock_ms += 20
        await original_send_audio_data(extension, audio_data)

    base_time = dt.datetime(2026, 1, 1)

    class ControlledDateTime:
        @classmethod
        def now(cls):
            return base_time + dt.timedelta(milliseconds=clock_ms)

    monkeypatch.setattr(
        gemini_tts_module,
        "genai",
        SimpleNamespace(Client=FakeClient),
    )
    monkeypatch.setattr(
        tts2_http.AsyncTTS2HttpExtension,
        "send_tts_audio_data",
        send_audio_data,
    )
    monkeypatch.setattr(tts2_http, "datetime", ControlledDateTime)
    tester = InterleavedTester()
    tester.set_test_mode_single(
        "gemini_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test-key",
                    "model": "gemini-3.8-flash-tts",
                }
            }
        ),
    )

    tester.run()

    assert request_texts == ["Hello", ", world", "Second"]
    assert tester.starts == ["first", "second"]
    assert [event["request_id"] for event in tester.ends] == [
        "first",
        "second",
    ]
    assert [
        event["request_total_audio_duration_ms"] for event in tester.ends
    ] == [200, 100]
    assert [event["request_event_interval_ms"] for event in tester.ends] == [
        40,
        20,
    ]
    assert tester.ttfb_turns == [1, 2]


@pytest.mark.parametrize(
    (
        "audio_cycles",
        "expected_starts",
        "expected_reasons",
        "expected_durations",
        "expected_intervals",
        "expected_error_count",
    ),
    [
        (
            (True, True),
            ["first", "second"],
            [1, 1],
            [100, 100],
            [20, 20],
            0,
        ),
        ((False, True), ["second"], [3, 1], [0, 100], [0, 20], 1),
        (("error", True), ["second"], [3, 1], [0, 100], [0, 20], 1),
        (
            ("transient", True),
            ["second"],
            [3, 1],
            [0, 100],
            [0, 20],
            1,
        ),
    ],
)
def test_extension_lifecycle_accounting_and_recovery(
    monkeypatch,
    tmp_path,
    audio_cycles,
    expected_starts,
    expected_reasons,
    expected_durations,
    expected_intervals,
    expected_error_count,
):
    runtime = pytest.importorskip("ten_runtime")
    if not hasattr(runtime, "ExtensionTester"):
        pytest.skip("TEN runtime test harness is not installed")
    from ten_ai_base import tts2_http
    from ten_ai_base.struct import TTSTextInput
    from ten_runtime import Data, ExtensionTester

    gemini_tts_module = import_module(
        "ten_packages.extension.gemini_tts_python.gemini_tts"
    )

    pcm = b"\x00\x01" * 2400
    clock_ms = 0
    request_index = 0

    class LifecycleTester(ExtensionTester):
        def __init__(self):
            super().__init__()
            self.starts = []
            self.ends = []
            self.errors = []
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
            self.send_request(ten_env_tester, "second")
            ten_env_tester.on_start_done()

        def on_audio_frame(self, _ten_env, audio_frame):
            buf = audio_frame.lock_buf()
            try:
                self.received_audio_bytes += len(buf)
            finally:
                audio_frame.unlock_buf(buf)

        def on_data(self, ten_env, data):
            name = data.get_name()
            if name == "error":
                payload, _ = data.get_property_to_json(None)
                self.errors.append(json.loads(payload))
                return
            if name == "tts_audio_start":
                payload, _ = data.get_property_to_json(None)
                self.starts.append(json.loads(payload)["request_id"])
                return
            if name != "tts_audio_end":
                return
            payload, _ = data.get_property_to_json(None)
            self.ends.append(json.loads(payload))
            if len(self.ends) == 2:
                ten_env.stop_test()

    class FakeAsyncInteractions:
        async def create(self, **kwargs):
            nonlocal clock_ms, request_index
            assert kwargs["store"] is False
            clock_ms += 10
            has_audio = audio_cycles[request_index]
            request_index += 1
            if has_audio == "error":
                raise RuntimeError("401 unauthorized")
            if has_audio == "transient":
                raise RuntimeError("429 rate limit")
            if not has_audio:
                return FakeAsyncStream([])
            return FakeAsyncStream(
                [
                    audio_event(base64.b64encode(pcm).decode("ascii")),
                ]
            )

    class FakeClient:
        def __init__(self, **_kwargs):
            self.aio = SimpleNamespace(
                interactions=FakeAsyncInteractions(),
                aclose=AsyncMock(),
            )

    original_send_audio_data = (
        tts2_http.AsyncTTS2HttpExtension.send_tts_audio_data
    )

    async def send_audio_data(extension, audio_data):
        nonlocal clock_ms
        clock_ms += 20
        return await original_send_audio_data(extension, audio_data)

    base_time = dt.datetime(2026, 1, 1)

    class ControlledDateTime:
        @classmethod
        def now(cls):
            return base_time + dt.timedelta(milliseconds=clock_ms)

    monkeypatch.setattr(
        gemini_tts_module,
        "genai",
        SimpleNamespace(Client=FakeClient),
    )
    monkeypatch.setattr(
        tts2_http.AsyncTTS2HttpExtension,
        "send_tts_audio_data",
        send_audio_data,
    )
    monkeypatch.setattr(tts2_http, "datetime", ControlledDateTime)
    tester = LifecycleTester()
    tester.set_test_mode_single(
        "gemini_tts_python",
        json.dumps(
            {
                "dump": True,
                "dump_path": str(tmp_path),
                "params": {
                    "api_key": "test-key",
                    "model": "gemini-3.8-flash-tts",
                    "voice": "Kore",
                    "language_code": "en-US",
                },
            }
        ),
    )

    tester.run()

    assert tester.starts == expected_starts
    assert len(tester.ends) == 2
    assert len(tester.errors) == expected_error_count
    emitted_cycles = sum(audio is True for audio in audio_cycles)
    assert tester.received_audio_bytes == len(pcm) * emitted_cycles
    assert [end["request_id"] for end in tester.ends] == [
        "first",
        "second",
    ]
    assert [end["reason"] for end in tester.ends] == expected_reasons
    assert [
        end["request_total_audio_duration_ms"] for end in tester.ends
    ] == expected_durations
    assert [
        end["request_event_interval_ms"] for end in tester.ends
    ] == expected_intervals
    for request_id, has_audio in zip(("first", "second"), audio_cycles):
        dump = tmp_path / f"gemini_dump_{request_id}.pcm"
        assert dump.exists()
        assert dump.read_bytes() == (pcm if has_audio is True else b"")
