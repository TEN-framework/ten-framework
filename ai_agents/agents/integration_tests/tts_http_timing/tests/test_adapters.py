"""Exercise real HTTP TTS adapters with delayed connection and audio responses."""

import asyncio
import base64
import json
import struct
from importlib import import_module
from types import SimpleNamespace

import httpx
import pytest
from integration_tests.tts_guarder.tests.test_metrics import (
    SequentialTTFBTester,
)

from .boundary import ProviderReference

SUPPORTED = {
    "generic_tts_http_python",
    "gemini_tts_python",
    "openai_tts2_python",
    "mistral_tts_python",
}


def test_http_adapter_ttfb_at_provider_boundary(extension_name, monkeypatch):
    if extension_name not in SUPPORTED:
        pytest.skip("This case covers the HTTPX TTS adapters only")
    import_module(f"ten_packages.extension.{extension_name}")
    reference = ProviderReference()
    pcm = base64.b64encode(b"\x01\x00" * 1600).decode()
    is_sse = extension_name in {
        "gemini_tts_python",
        "mistral_tts_python",
    }

    class AudioStream(httpx.AsyncByteStream):
        async def __aiter__(self):
            if is_sse:
                yield b'data: {"status":"started"}\n\n'
            await asyncio.sleep(0.02)
            if extension_name == "mistral_tts_python":
                encoded = base64.b64encode(
                    struct.pack("<f", 0.1) * 800
                ).decode()
                payload = {"type": "speech.audio.delta", "audio_data": encoded}
            elif extension_name == "gemini_tts_python":
                payload = {
                    "event_type": "step.delta",
                    "index": 0,
                    "delta": {
                        "type": "audio",
                        "data": pcm,
                        "mime_type": "audio/l16",
                        "sample_rate": 24000,
                        "channels": 1,
                    },
                }
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
            if extension_name == "gemini_tts_python":
                completed = json.dumps(
                    {
                        "event_type": "interaction.completed",
                        "interaction": {"status": "completed"},
                    }
                ).encode()
                yield b"data: " + completed + b"\n\n"

    class Provider(httpx.AsyncBaseTransport):
        async def handle_async_request(self, request):
            reference.text_sent()
            await asyncio.sleep(0.05)
            trace = request.extensions.get("trace")
            if trace:
                await trace("http11.send_request_body.started", {})
            if is_sse:
                content_type = "text/event-stream"
            else:
                content_type = "audio/pcm"
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
        "generic_tts_http_python": "generic_tts",
        "gemini_tts_python": "gemini_tts",
        "openai_tts2_python": "openai_tts",
        "mistral_tts_python": "mistral_tts",
    }
    module = import_module(
        f"ten_packages.extension.{extension_name}.{filenames[extension_name]}"
    )
    sync_client = None
    if extension_name == "gemini_tts_python":
        original_genai = module.genai
        async_client = Client()
        sync_client = httpx.Client(
            transport=httpx.MockTransport(
                lambda request: httpx.Response(500, request=request)
            )
        )

        class GeminiClient:
            def __init__(self, *args, **kwargs):
                http_options = dict(kwargs.pop("http_options", {}) or {})
                http_options["httpx_client"] = sync_client
                http_options["httpx_async_client"] = async_client
                self.inner = original_genai.Client(
                    *args,
                    http_options=http_options,
                    **kwargs,
                )
                self.aio = self.inner.aio

            def __getattr__(self, name):
                return getattr(self.inner, name)

        monkeypatch.setattr(
            module,
            "genai",
            SimpleNamespace(Client=GeminiClient),
        )
    else:
        monkeypatch.setattr(module, "AsyncClient", Client)
    config = {
        "url": "https://mock.example/tts",
        "api_key": "mock",
        "params": {"api_key": "mock"},
    }
    if extension_name == "mistral_tts_python":
        config["params"]["model"] = "mock"
    from integration_tests.tts_guarder.tests.ttfb_reference import (
        install_ttfb_reference,
    )

    with install_ttfb_reference(
        extension_name, config, monkeypatch
    ) as hook_reference:
        tester = SequentialTTFBTester(hook_reference)
        # The fixture separately timestamps provider sends to audit the hook.
        original_begin = hook_reference.begin_request

        def begin(request_id):
            original_begin(request_id)
            reference.begin_request(request_id)

        monkeypatch.setattr(hook_reference, "begin_request", begin)
        tester.set_test_mode_single(extension_name, json.dumps(config))
        error = tester.run()
        assert error is None, error.error_message() if error else ""
        for request_id in tester.request_ids:
            # Adapter timing also includes parsing/conversion after wire audio.
            assert hook_reference.elapsed_ms(
                request_id
            ) >= reference.elapsed_ms(request_id)
    if sync_client:
        sync_client.close()


pytestmark = pytest.mark.api
