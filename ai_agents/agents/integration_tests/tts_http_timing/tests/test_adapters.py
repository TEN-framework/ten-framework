"""Exercise real HTTP TTS adapters with delayed connection and audio responses."""

import asyncio
import base64
import io
import json
from importlib import import_module
import struct
import wave

import httpx
import pytest

from .boundary import SequentialTTFBTester, ProviderReference

SUPPORTED = {
    "openai_tts2_python",
    "mistral_tts_python",
    "smallest_tts_python",
    "ezai_tw_tts_python",
    "sarvam_http_tts",
}


def _wav(width):
    output = io.BytesIO()
    with wave.open(output, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(width)
        audio.setframerate(16000)
        audio.writeframes(b"\x01" + b"\x00" * (width - 1))
    return output.getvalue()


def test_http_adapter_ttfb_at_provider_boundary(extension_name, monkeypatch):
    if extension_name not in SUPPORTED:
        pytest.skip("This case covers the HTTPX TTS adapters only")
    import_module(f"ten_packages.extension.{extension_name}")
    reference = ProviderReference()
    pcm = base64.b64encode(b"\x01\x00" * 1600).decode()
    wav16 = base64.b64encode(_wav(2)).decode()
    wav24 = base64.b64encode(_wav(3)).decode()
    is_sse = extension_name in {
        "mistral_tts_python",
        "smallest_tts_python",
    }

    class AudioStream(httpx.AsyncByteStream):
        async def __aiter__(self):
            if is_sse:
                yield b'data: {"status":"started"}\n\n'
            await asyncio.sleep(0.02)
            if extension_name == "mistral_tts_python":
                encoded = base64.b64encode(struct.pack("<f", 0.1) * 800).decode()
                payload = {"type": "speech.audio.delta", "audio_data": encoded}
            elif extension_name == "smallest_tts_python":
                payload = {"audio": pcm}
            elif extension_name == "sarvam_http_tts":
                payload = {"audios": [wav16]}
            elif extension_name == "ezai_tw_tts_python":
                payload = {"audio": wav24}
            else:
                reference.audio_received()
                yield b"\x01\x00" * 1600
                await asyncio.sleep(0.01)
                yield b"\x02\x00" * 1600
                return
            encoded = json.dumps(payload).encode()
            reference.audio_received()
            yield b"data: " + encoded + b"\n\n" if is_sse else encoded
            await asyncio.sleep(0.01)

    class Provider(httpx.AsyncBaseTransport):
        async def handle_async_request(self, request):
            await asyncio.sleep(0.05)
            reference.text_sent()
            trace = request.extensions.get("trace")
            if trace:
                await trace("http11.send_request_body.started", {})
            content_type = (
                "text/event-stream"
                if is_sse
                else (
                    "application/json"
                    if extension_name in {"sarvam_http_tts", "ezai_tw_tts_python"}
                    else "audio/pcm"
                )
            )
            return httpx.Response(
                200,
                headers={"content-type": content_type},
                stream=AudioStream(),
            )

    original = httpx.AsyncClient

    class Client(original):
        def __init__(self, *args, **kwargs):
            kwargs["transport"] = Provider()
            super().__init__(*args, **kwargs)

    monkeypatch.setattr(httpx, "AsyncClient", Client)
    filenames = {
        "openai_tts2_python": "openai_tts",
        "mistral_tts_python": "mistral_tts",
        "smallest_tts_python": "smallest_tts",
        "sarvam_http_tts": "sarvam_tts",
        "ezai_tw_tts_python": "ezai_tts",
    }
    module = import_module(
        f"ten_packages.extension.{extension_name}.{filenames[extension_name]}"
    )
    monkeypatch.setattr(module, "AsyncClient", Client)
    config = {
        "url": "https://mock.example/tts",
        "api_key": "mock",
        "params": {"api_key": "mock"},
    }
    if extension_name == "smallest_tts_python":
        config["params"]["voice_id"] = "mock"
    elif extension_name == "mistral_tts_python":
        config["params"]["model"] = "mock"
    elif extension_name == "sarvam_http_tts":
        config["params"] = {
            "api_subscription_key": "mock",
            "target_language_code": "hi-IN",
        }
    tester = SequentialTTFBTester(reference)
    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()
    assert error is None, error.error_message() if error else ""


pytestmark = pytest.mark.api
