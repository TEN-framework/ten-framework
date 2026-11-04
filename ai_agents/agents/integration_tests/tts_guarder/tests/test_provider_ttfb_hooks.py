"""Public transport/SDK fixture checks for every second-stage provider hook."""

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
