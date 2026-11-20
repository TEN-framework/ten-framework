import array
import json
import os

import httpx
import pytest

from ten_packages.extension.gnani_tts_python.config import GnaniTTSConfig
from ten_packages.extension.gnani_tts_python.gnani_tts import Event, GnaniTTSClient


async def synthesize(response, text="hello"):
    config = GnaniTTSConfig(params={"api_key": "unit-secret"})
    client = GnaniTTSClient(config.params)
    await client.http.aclose()

    def handle(request):
        assert request.headers["X-API-Key-ID"] == "unit-secret"
        payload = json.loads(request.content)
        assert payload["audio_config"]["container"] == "raw"
        assert payload["model"] == "timbre-v3.0"
        return response

    client.http = httpx.AsyncClient(transport=httpx.MockTransport(handle))
    try:
        return [item async for item in client.get(text, "one")]
    finally:
        await client.clean()


@pytest.mark.asyncio
async def test_streams_aligned_pcm_and_skips_control_events():
    body = (
        "event: start\ndata: {}\n\n"
        'event: chunk\ndata: {"audio":"AQ=="}\n\n'
        'event: chunk\ndata: {"audio":"AgME"}\n\n'
        "event: complete\ndata: {}\n\n"
    )
    assert await synthesize(httpx.Response(200, text=body)) == [
        (bytes([1, 2, 3, 4]), Event.RESPONSE),
        (None, Event.END),
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "body",
    [
        "event: start\ndata: {}\n\n",
        "event: error\ndata: {}\n\n",
        'event: chunk\ndata: {"audio":"??"}\n\n',
        'event: chunk\ndata: {"audio":"AQ=="}\n\nevent: complete\ndata: {}\n\n',
    ],
)
async def test_malformed_or_truncated_stream(body):
    results = await synthesize(httpx.Response(200, text=body))
    assert results[-1][1] == Event.ERROR


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "status,expected",
    [
        (401, Event.INVALID_KEY_ERROR),
        (403, Event.INVALID_KEY_ERROR),
        (503, Event.ERROR),
    ],
)
async def test_http_errors(status, expected):
    assert await synthesize(httpx.Response(status, text="unit-secret")) == [
        (f"Gnani HTTP {status}".encode(), expected)
    ]


@pytest.mark.asyncio
async def test_blank_text_ends_without_network():
    assert await synthesize(httpx.Response(500), " ") == [(None, Event.END)]


def test_config_redacts_key_and_rejects_invalid_settings():
    assert (
        "unit-secret" not in GnaniTTSConfig(params={"api_key": "unit-secret"}).to_str()
    )
    with pytest.raises(ValueError):
        GnaniTTSConfig().validate()
    with pytest.raises(ValueError):
        GnaniTTSConfig(params={"sample_rate": 1})


@pytest.mark.asyncio
@pytest.mark.skipif(not os.getenv("GNANI_API_KEY"), reason="requires Gnani credential")
@pytest.mark.parametrize(
    "model,voice,language,text",
    [
        ("timbre-v2.5", "Nalini", "hi-IN", "नमस्ते, आपका दिन कैसा है?"),
        ("timbre-v3.0", "Shivani", "en-IN", "Welcome to the voice assistant."),
    ],
)
async def test_live_speech(model, voice, language, text):
    config = GnaniTTSConfig(
        params={
            "api_key": os.environ["GNANI_API_KEY"],
            "model": model,
            "voice": voice,
            "language": language,
        }
    )
    client = GnaniTTSClient(config.params)
    try:
        results = [event async for event in client.get(text, "live")]
    finally:
        await client.clean()
    assert results[-1] == (None, Event.END)
    assert all(kind == Event.RESPONSE for _, kind in results[:-1])
    pcm = b"".join(audio for audio, _ in results[:-1])
    assert len(pcm) > 4800 and not pcm.startswith(b"RIFF")
    assert max(array.array("h", pcm)) > 100


@pytest.mark.asyncio
async def test_retry_rate_limit_before_audio():
    responses = iter(
        [
            httpx.Response(429, headers={"Retry-After": "0"}),
            httpx.Response(
                200,
                text='event: chunk\ndata: {"audio":"AQI="}\n\nevent: complete\ndata: {}\n\n',
            ),
        ]
    )
    config = GnaniTTSConfig(params={"api_key": "test"})
    client = GnaniTTSClient(config.params)
    await client.http.aclose()
    client.http = httpx.AsyncClient(
        transport=httpx.MockTransport(lambda _: next(responses))
    )
    try:
        assert [item async for item in client.get("Hello", "test")] == [
            (b"\x01\x02", Event.RESPONSE),
            (None, Event.END),
        ]
    finally:
        await client.clean()


@pytest.mark.asyncio
async def test_flush_awaits_the_inflight_provider_task(monkeypatch):
    import asyncio
    from ten_ai_base.tts2_http import AsyncTTS2HttpExtension
    from ten_packages.extension.gnani_tts_python.extension import GnaniTTSExtension

    started, stopped = asyncio.Event(), asyncio.Event()

    async def stalled_request(self, text_input):
        started.set()
        try:
            await asyncio.Event().wait()
        finally:
            stopped.set()

    monkeypatch.setattr(AsyncTTS2HttpExtension, "request_tts", stalled_request)
    extension = GnaniTTSExtension("gnani")
    request = asyncio.create_task(extension.request_tts(None))
    await asyncio.wait_for(started.wait(), 1)
    await extension._cancel_current_task()
    assert stopped.is_set()
    with pytest.raises(asyncio.CancelledError):
        await request
    assert extension.current_task is None
