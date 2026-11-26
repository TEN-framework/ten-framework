"""Public transport/SDK fixture checks for every second-stage provider hook."""

import asyncio
import io
import struct
import wave
from importlib import import_module
from types import SimpleNamespace

import pytest

from .provider_ttfb_hooks import install_hook
from .provider_ttfb_hooks.reference import Reference


@pytest.fixture
def clock(monkeypatch):
    now = [0]
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.reference.time.monotonic_ns", lambda: now[0]
    )
    return now


def test_google_public_lazy_iterator(monkeypatch, clock):
    module = import_module(
        "ten_packages.extension.google_tts_python.google_tts"
    )

    class Client:
        def streaming_synthesize(self, requests):
            clock[0] = 150_000_000
            list(requests)
            yield SimpleNamespace(audio_content=b"")
            clock[0] = 170_000_000
            yield SimpleNamespace(audio_content=b"audio")

    monkeypatch.setattr(module.texttospeech, "TextToSpeechClient", Client)
    with install_hook("google_tts_python", {}, monkeypatch) as reference:
        reference.begin_request("first")
        outputs = list(
            module.texttospeech.TextToSpeechClient().streaming_synthesize(
                [
                    SimpleNamespace(input=SimpleNamespace(text="")),
                    SimpleNamespace(input=SimpleNamespace(text="Hello")),
                ]
            )
        )
        assert [item.audio_content for item in outputs] == [b"", b"audio"]
        assert reference.elapsed_ms("first") == 20
    assert module.texttospeech.TextToSpeechClient is Client


def test_gmicloud_http_boundary(monkeypatch, clock):
    module = import_module(
        "ten_packages.extension.gmicloud_tts2_python.client"
    )

    class FakeResponse:
        def __init__(self, status_code=200, json_data=None, content=b""):
            self.status_code = status_code
            self._json = json_data or {}
            self.content = content

        def json(self):
            return self._json

        @property
        def text(self):
            return ""

    def make_wav():
        buf = io.BytesIO()
        with wave.open(buf, "wb") as pcm:
            pcm.setnchannels(1)
            pcm.setsampwidth(2)
            pcm.setframerate(24000)
            pcm.writeframes(struct.pack("<2h", 1, -1))
        return buf.getvalue()

    state = {"download": make_wav()}

    async def fake_post(url, json=None, headers=None):
        clock[0] += 10_000_000
        return FakeResponse(
            json_data={"status": "success",
                       "outcome": {"audio_url": "https://a/wav"}}
        )

    async def fake_get(url, **kwargs):
        clock[0] += 15_000_000
        return FakeResponse(content=state["download"])

    options = module.GmicloudTTSOptions(
        api_key="k",
        base_url="https://console.gmicloud.ai",
        model="inworld-tts-2",
        voice="Dennis",
        audio_format="WAV",
        speed=1.0,
        sample_rate=24000,
        text_chunk_chars=1000,
        request_timeout_sec=5.0,
        poll_interval_sec=0.01,
        poll_attempts=3,
    )
    with install_hook("gmicloud_tts2_python", {}, monkeypatch) as reference:
        client = module.GmicloudTTSClient(options)
        client._client.post = fake_post
        client._client.get = fake_get

        def run(text, request_id):
            async def collect():
                return [event async for event in client.get(text, request_id)]
            return asyncio.run(collect())

        # sequential requests: each measures submission -> WAV arrival at
        # the HTTP boundary (10ms submit + 15ms download = 25ms)
        reference.begin_request("first")
        events = run("Hello", "first")
        assert events[-1][1].name == "END"
        assert reference.elapsed_ms("first") == 25

        clock[0] += 100_000_000
        reference.begin_request("second")
        run("Again", "second")
        assert reference.elapsed_ms("second") == 25

        # an empty WAV arrival is not audio and must not close the interval
        state["download"] = b""
        clock[0] += 100_000_000
        reference.begin_request("empty")
        run("Hello", "empty")
        assert reference.elapsed_ms("empty") is None

        asyncio.run(client.clean())


def test_reference_uses_capture_request_and_ignores_empty_chunks(clock):
    reference = Reference()
    reference.begin_request("first")
    reference.capture("text")
    clock[0] = 5_000_000
    reference.capture("audio", b"")
    clock[0] = 20_000_000
    reference.capture("audio", b"audio")
    reference.begin_request("second")
    clock[0] = 100_000_000
    reference.capture("text")
    clock[0] = 125_000_000
    reference.capture("audio", b"audio")
    assert reference.sent == {} and reference.audio == {}
    assert reference.elapsed_ms("first") == 20
    assert reference.elapsed_ms("second") == 25


def test_azure_mock_reference_uses_sdk_timestamps(monkeypatch):
    from .provider_ttfb_hooks.azure import Hook

    class Function:
        def __init__(self, callback):
            self.callback = callback

        def __call__(self, *args):
            return self.callback(*args)

    count = [0]
    sent = [100_000_000, 200_000_000, 300_000_000]
    received = [120_000_000, 240_000_000, 330_000_000]
    library = SimpleNamespace(
        azure_mock_synthesis_count=Function(lambda: count[0]),
        azure_mock_text_sent_ns=Function(lambda index: sent[index]),
        azure_mock_first_audio_ns=Function(lambda index: received[index]),
    )
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.azure.ctypes.CDLL", lambda path: library
    )
    reference = Hook().install(
        None, None, {"ttfb_mock_sdk_library": "/mock/library"}
    )
    reference.begin_request("first")
    count[0] = (
        2  # First request had two text chunks; retain its first SDK call.
    )
    reference.begin_request("second")
    assert reference.elapsed_ms("first") == 20
    assert reference.elapsed_ms("second") == 30
    assert reference.elapsed_ms("missing") is None


def test_azure_live_sdk_is_not_claimed_as_independent_reference():
    from .provider_ttfb_hooks.azure import Hook

    with pytest.raises(ValueError, match="cannot be intercepted"):
        Hook().install(None, None, {})


@pytest.mark.asyncio
async def test_http_adapter_hook_tracks_input_and_first_audio(
    monkeypatch, clock
):
    from .provider_ttfb_hooks.http import HttpAdapterHook
    from ten_ai_base.struct import TTS2HttpResponseEventType as Event

    class Client:
        async def get(self, text, request_id):
            clock[0] = 150_000_000
            yield b"", Event.RESPONSE
            clock[0] = 170_000_000
            yield b"audio", Event.RESPONSE
            clock[0] = 270_000_000
            yield b"tail", Event.RESPONSE
            yield None, Event.END

    module = SimpleNamespace(Client=Client)
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.http.import_module", lambda name: module
    )
    hook = HttpAdapterHook()
    hook.client_class = "Client"
    reference = Reference()
    reference.begin_request("first")
    hook.install(monkeypatch, reference, {})
    stream = Client().get("Hello", "first")
    outputs = [event async for event in stream]
    assert outputs == [
        (b"", Event.RESPONSE),
        (b"audio", Event.RESPONSE),
        (b"tail", Event.RESPONSE),
        (None, Event.END),
    ]
    assert reference.elapsed_ms("first") == 170


def test_cosy_hook_observes_sdk_pool_reuse(monkeypatch, clock):
    from .provider_ttfb_hooks.cosy import Hook

    received = []

    class Synthesizer:
        def streaming_call(self, text):
            clock[0] = 20_000_000
            self.callback.on_data(b"audio")

    class Pool:
        def __init__(self):
            self.synthesizer = Synthesizer()

        def borrow_synthesizer(self, callback):
            self.synthesizer.callback = callback
            return self.synthesizer

    module = SimpleNamespace(
        SpeechSynthesizer=Synthesizer,
        SpeechSynthesizerObjectPool=Pool,
        ResultCallback=object,
    )
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.cosy.import_module", lambda name: module
    )
    pool = (
        Pool()
    )  # The instance predates hook installation, as in the SDK pool.
    reference = Reference()
    reference.begin_request("first")
    Hook().install(monkeypatch, reference, {})
    pool.borrow_synthesizer(callback=SimpleNamespace(on_data=received.append))
    pool.synthesizer.streaming_call("Hello")
    assert received == [b"audio"]
    assert reference.elapsed_ms("first") == 20


def test_cosy_hook_binds_late_audio_to_original_callback(monkeypatch, clock):
    from .provider_ttfb_hooks.cosy import Hook

    class Synthesizer:
        def streaming_call(self, text):
            self.callback.on_data(text.encode())

    class Pool:
        def __init__(self):
            self.synthesizer = Synthesizer()

        def borrow_synthesizer(self, callback):
            self.synthesizer.callback = callback
            return self.synthesizer

    module = SimpleNamespace(
        SpeechSynthesizer=Synthesizer,
        SpeechSynthesizerObjectPool=Pool,
        ResultCallback=object,
    )
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.cosy.import_module", lambda name: module
    )
    pool = Pool()
    reference = Reference()
    Hook().install(monkeypatch, reference, {})

    reference.begin_request("first")
    old_callback = pool.borrow_synthesizer(
        callback=SimpleNamespace(request_id="first", on_data=lambda data: None)
    ).callback
    reference.capture("text", "first")
    clock[0] = 20_000_000
    old_callback.on_data(b"old-audio")

    reference.begin_request("second")
    new_callback = pool.borrow_synthesizer(
        callback=SimpleNamespace(request_id="second", on_data=lambda data: None)
    ).callback
    reference.capture("text", "second")
    clock[0] = 30_000_000
    old_callback.on_data(b"late-old-audio")
    assert reference.elapsed_ms("second") is None
    new_callback.on_data(b"new-audio")
    assert reference.elapsed_ms("first") == 20
    assert reference.elapsed_ms("second") == 30
