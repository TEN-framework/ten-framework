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
async def test_ttfb_spans_text_batches_and_precedes_sample_alignment(monkeypatch):
    from types import SimpleNamespace
    from ten_packages.extension.gnani_tts_python import gnani_tts

    now = 0
    monkeypatch.setattr(
        gnani_tts, "time", SimpleNamespace(monotonic_ns=lambda: now * 1_000_000)
    )

    class Stream(httpx.AsyncByteStream):
        def __init__(self, text):
            self.text = text

        async def __aiter__(self):
            nonlocal now
            if self.text != "empty":
                now += 7
                yield b'event: chunk\ndata: {"audio":"AQ=="}\n\n'
                now += 100
                yield b'event: chunk\ndata: {"audio":"Ag=="}\n\n'
            yield b"event: complete\ndata: {}\n\n"

    def handle(request):
        return httpx.Response(200, stream=Stream(json.loads(request.content)["text"]))

    client = GnaniTTSClient(GnaniTTSConfig(params={"api_key": "test"}).params)
    await client.http.aclose()
    client.http = httpx.AsyncClient(transport=httpx.MockTransport(handle))
    try:
        assert [item async for item in client.get("empty", "first")] == [
            (None, Event.END)
        ]
        now += 20
        for request_id, expected_ms in [("first", 27), ("next", 7)]:
            assert [item async for item in client.get("Hello", request_id)] == [
                (bytes([1, 2]), Event.RESPONSE),
                (None, Event.END),
            ]
            assert client.first_audio_ms == expected_ms
    finally:
        await client.clean()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "message,expected",
    [
        (
            "Invalid text or audio configuration.",
            "Gnani synthesis error: invalid text or audio configuration",
        ),
        (
            "Missing sample_rate: unit-secret",
            "Gnani synthesis error: missing sample_rate",
        ),
        (
            "Unexpected error at https://example.com/?token=unit-secret",
            "Gnani reported a synthesis error",
        ),
        (None, "Gnani reported a synthesis error"),
    ],
)
async def test_safe_sse_diagnostic(message, expected):
    body = "event: error\ndata: " + json.dumps({"message": message}) + "\n\n"
    assert await synthesize(httpx.Response(200, text=body)) == [
        (expected.encode(), Event.ERROR)
    ]
