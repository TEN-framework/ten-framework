"""Public transport/SDK fixture checks for every second-stage provider hook."""

import asyncio
import base64
import io
import json
import wave
from importlib import import_module
from types import SimpleNamespace

import httpx
import pytest

from .provider_ttfb_hooks import HOOKS, install_hook
from .provider_ttfb_hooks.reference import Reference


@pytest.fixture
def clock(monkeypatch):
    now = [0]
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.reference.time.monotonic_ns", lambda: now[0]
    )
    return now


def wav_bytes():
    output = io.BytesIO()
    with wave.open(output, "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(16000)
        stream.writeframes(b"\x01\x00")
    return output.getvalue()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "name,content_type,key",
    [
        ("openai_tts2_python", "audio/pcm", None),
        ("mistral_tts_python", "text/event-stream", "audio_data"),
        ("smallest_tts_python", "text/event-stream", "audio"),
        ("ezai_tw_tts_python", "application/json", "audio"),
        ("sarvam_http_tts", "application/json", "audios"),
        ("humeai_tts_python", "application/json", "audio"),
    ],
)
async def test_http_hook_preserves_data_and_defers_recognition(
    monkeypatch, clock, name, content_type, key
):
    module = import_module(f".provider_ttfb_hooks.{HOOKS[name]}", __package__)
    hook = module.Hook()
    target = import_module(hook.module_name)
    original = target.AsyncClient if hook.binding == "AsyncClient" else target.httpx
    response_bytes = b""

    class Stream(httpx.AsyncByteStream):
        async def __aiter__(self):
            nonlocal response_bytes
            clock[0] = 155_000_000
            if key:
                empty = json.dumps({key: [] if key == "audios" else ""}).encode()
                if "event-stream" in content_type:
                    empty = b"data: " + empty + b"\n\n"
                elif "json" in content_type:
                    empty += b"\n"
                response_bytes += empty
                yield empty
            clock[0] = 170_000_000
            audio = "AQA="
            body = (
                json.dumps({key: [audio] if key == "audios" else audio}).encode()
                if key
                else b"\x01\x00"
            )
            if "event-stream" in content_type:
                body = b"data: " + body + b"\n\n"
            response_bytes += body
            yield body

    class Transport(httpx.AsyncBaseTransport):
        async def handle_async_request(self, request):
            clock[0] = 150_000_000
            await request.extensions["trace"]("http11.send_request_body.started", {})
            return httpx.Response(
                200, headers={"content-type": content_type}, stream=Stream()
            )

    with install_hook(name, {}, monkeypatch) as reference:
        reference.begin_request("first")
        client_class = (
            target.AsyncClient
            if hook.binding == "AsyncClient"
            else target.httpx.AsyncClient
        )
        async with client_class(transport=Transport()) as client:
            response = await client.post("https://local.invalid/tts", content="Hello")
            assert response.content == response_bytes
        assert reference.audio == {}  # No parsing happened in the receive callback.
        assert reference.elapsed_ms("first") == 20
    assert (
        target.AsyncClient if hook.binding == "AsyncClient" else target.httpx
    ) is original


def test_google_public_lazy_iterator(monkeypatch, clock):
    module = import_module("ten_packages.extension.google_tts_python.google_tts")

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


def test_polly_public_call_and_audio_stream(monkeypatch, clock):
    module = import_module("ten_packages.extension.polly_tts.polly_tts")
    closed = []

    class Stream:
        def iter_chunks(self, chunk_size):
            yield b""
            clock[0] = 20_000_000
            yield b"audio"
            clock[0] = 120_000_000
            yield b"tail"

        def close(self):
            closed.append(True)

    class Client:
        def synthesize_speech(self, **kwargs):
            return {"AudioStream": Stream()}

    class Session:
        def client(self, **kwargs):
            return Client()

    monkeypatch.setattr(module.boto3, "Session", Session)
    with install_hook("polly_tts", {}, monkeypatch) as reference:
        reference.begin_request("first")
        stream = (
            module.boto3.Session()
            .client()
            .synthesize_speech(Text="Hello")["AudioStream"]
        )
        assert list(stream.iter_chunks(chunk_size=2)) == [b"", b"audio", b"tail"]
        stream.close()
        assert reference.elapsed_ms("first") == 20
    assert closed == [True]


def test_oracle_ignores_wav_header_and_complete_body_wait(monkeypatch, clock):
    module = import_module("ten_packages.extension.oracle_tts_python.oracle_tts")
    audio = wav_bytes()

    class Body:
        def iter_content(self, chunk_size):
            clock[0] = 5_000_000
            yield audio[:44]
            clock[0] = 20_000_000
            yield audio[44:]
            clock[0] = 120_000_000

    class Client:
        def synthesize_speech(self, **kwargs):
            return SimpleNamespace(data=Body())

    monkeypatch.setattr(module.oci.ai_speech, "AIServiceSpeechClient", Client)
    with install_hook("oracle_tts_python", {}, monkeypatch) as reference:
        reference.begin_request("first")
        response = module.oci.ai_speech.AIServiceSpeechClient().synthesize_speech(
            synthesize_speech_details=SimpleNamespace(text="Hello")
        )
        assert b"".join(response.data.iter_content(chunk_size=4096)) == audio
        assert reference.elapsed_ms("first") == 20


def test_cosy_public_callback_is_forwarded(monkeypatch, clock):
    module = import_module("ten_packages.extension.cosy_tts_python.cosy_tts")
    received = []

    class Synthesizer:
        def __init__(self, callback):
            self.callback = callback

        def streaming_call(self, text):
            self.callback.on_data(b"")
            clock[0] = 20_000_000
            self.callback.on_data(b"audio")
            clock[0] = 120_000_000
            self.callback.on_complete()

    monkeypatch.setattr(module, "SpeechSynthesizer", Synthesizer)
    callback = SimpleNamespace(
        on_data=received.append, on_complete=lambda: received.append("done")
    )
    with install_hook("cosy_tts_python", {}, monkeypatch) as reference:
        reference.begin_request("first")
        module.SpeechSynthesizer(callback=callback).streaming_call("Hello")
        assert received == [b"", b"audio", "done"]
        assert reference.elapsed_ms("first") == 20


@pytest.mark.asyncio
async def test_typecast_trace_and_sdk_chunks(monkeypatch, clock):
    module = import_module("ten_packages.extension.typecast_tts_python.typecast_tts")

    class Session:
        def __init__(self, trace_configs):
            self.traces = trace_configs

    class Client:
        def __init__(self, session):
            self.session = session

        async def text_to_speech_stream(self):
            clock[0] = 150_000_000
            for trace in self.session.traces:
                for callback in trace.on_request_chunk_sent:
                    await callback(self.session, None, SimpleNamespace(chunk=b"Hello"))
            yield b""
            clock[0] = 170_000_000
            yield b"audio"

    monkeypatch.setattr(module.aiohttp, "ClientSession", Session)
    monkeypatch.setattr(module, "AsyncTypecast", Client)
    with install_hook("typecast_tts_python", {}, monkeypatch) as reference:
        reference.begin_request("first")
        session = module.aiohttp.ClientSession()
        chunks = [
            chunk
            async for chunk in module.AsyncTypecast(
                session=session
            ).text_to_speech_stream()
        ]
        assert chunks == [b"", b"audio"]
        assert reference.elapsed_ms("first") == 20


@pytest.mark.asyncio
@pytest.mark.parametrize("name", ["murf_tts_python", "stepfun_tts_python"])
async def test_websocket_hook_uses_wire_payloads(monkeypatch, name):
    import websockets

    module_name = "murf_tts" if name == "murf_tts_python" else "stepfun_tts"
    module = import_module(f"ten_packages.extension.{name}.{module_name}")
    original = module.websockets
    audio = base64.b64encode(wav_bytes()).decode()
    outgoing = (
        {"context_id": "first", "text": "Hello"}
        if name == "murf_tts_python"
        else {"type": "tts.text.delta", "data": {"text": "Hello"}}
    )
    incoming = (
        {"context_id": "first", "audio": audio}
        if name == "murf_tts_python"
        else {"type": "tts.response.audio.delta", "data": {"audio": audio}}
    )

    async def provider(socket):
        assert json.loads(await socket.recv()) == outgoing
        await socket.send(json.dumps({"type": "control", "audio": ""}))
        await asyncio.sleep(0.02)
        await socket.send(json.dumps(incoming))
        await socket.wait_closed()

    async with websockets.serve(provider, "127.0.0.1", 0) as server:
        url = f"ws://127.0.0.1:{server.sockets[0].getsockname()[1]}"
        with install_hook(name, {"url": url}, monkeypatch) as reference:
            reference.begin_request("first")
            async with module.websockets.connect(url) as socket:
                await socket.send(json.dumps(outgoing))
                await socket.recv()
                assert reference.elapsed_ms("first") is None
                assert json.loads(await socket.recv()) == incoming
                assert reference.elapsed_ms("first") >= 15
    assert module.websockets is original


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


def test_murf_context_mismatch_is_excluded():
    from .provider_ttfb_hooks.murf import Hook

    assert not Hook.is_audio(json.dumps({"context_id": "old", "audio": "AQA="}), "new")
    assert not Hook.is_text(json.dumps({"context_id": "old", "text": "Hello"}), "new")
    assert Hook.is_audio(json.dumps({"context_id": "new", "audio": "AQA="}), "new")


def test_oracle_detector_waits_for_fragmented_wav_audio():
    from .provider_ttfb_hooks.oracle import AudioDetector

    audio = wav_bytes()
    detector = AudioDetector()
    assert not detector.feed(audio[:2])
    assert not detector.feed(audio[2:16])
    assert not detector.feed(audio[16:44])
    assert detector.feed(audio[44:])


@pytest.mark.asyncio
@pytest.mark.parametrize("name", ["generic_tts_http_python", "gemini_tts_python"])
async def test_additional_http_hooks(monkeypatch, clock, name):
    spec = import_module(f".provider_ttfb_hooks.{HOOKS[name]}", __package__)
    hook = spec.Hook()
    target = import_module(hook.module_name)
    original = target.AsyncClient if hook.binding == "AsyncClient" else target.httpx
    gemini = name == "gemini_tts_python"

    class Stream(httpx.AsyncByteStream):
        async def __aiter__(self):
            clock[0] = 155_000_000
            if gemini:
                yield b'data: {"candidates":[{"content":{"parts":[{"inlineData":{"mimeType":"image/png","data":"AQA="}}]}}]}\n\n'
            clock[0] = 170_000_000
            if gemini:
                yield b'data: {"candidates":[{"content":{"parts":[{"inlineData":{"mimeType":"audio/L16;rate=24000","data":"AQA="}}]}}]}\n\n'
            else:
                yield b"\x01\x00"

    class Transport(httpx.AsyncBaseTransport):
        async def handle_async_request(self, request):
            clock[0] = 150_000_000
            await request.extensions["trace"]("http11.send_request_body.started", {})
            return httpx.Response(
                200,
                headers={
                    "content-type": "text/event-stream" if gemini else "audio/pcm"
                },
                stream=Stream(),
            )

    with install_hook(name, {}, monkeypatch) as reference:
        reference.begin_request("first")
        client_type = (
            target.AsyncClient
            if hook.binding == "AsyncClient"
            else target.httpx.AsyncClient
        )
        async with client_type(transport=Transport()) as client:
            response = await client.post("https://local.invalid/tts", content="Hello")
            assert response.content
        assert reference.audio == {}
        assert reference.elapsed_ms("first") == 20
    assert (
        target.AsyncClient if hook.binding == "AsyncClient" else target.httpx
    ) is original


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
    reference = Hook().install(None, None, {"ttfb_mock_sdk_library": "/mock/library"})
    reference.begin_request("first")
    count[0] = 2  # First request had two text chunks; retain its first SDK call.
    reference.begin_request("second")
    assert reference.elapsed_ms("first") == 20
    assert reference.elapsed_ms("second") == 30
    assert reference.elapsed_ms("missing") is None


def test_azure_live_sdk_is_not_claimed_as_independent_reference():
    from .provider_ttfb_hooks.azure import Hook

    with pytest.raises(ValueError, match="cannot be intercepted"):
        Hook().install(None, None, {})
