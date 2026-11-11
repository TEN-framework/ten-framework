"""Streaming lifecycle tests for the Breeze HTTP client."""

from __future__ import annotations

import asyncio

import pytest

from breeze_tts import (
    BYTES_PER_SAMPLE,
    SAMPLE_RATE,
    BreezeTTSClient,
    BreezeTTSParams,
)
from ten_ai_base.struct import TTS2HttpResponseEventType


class FakeContent:
    def __init__(self, chunks):
        self._chunks = chunks

    async def iter_chunked(self, _size):
        for chunk in self._chunks:
            yield chunk


class FakeResponse:
    def __init__(self, status=200, chunks=(), text=""):
        self.status = status
        self.content = FakeContent(chunks)
        self._text = text
        self.closed = False

    async def text(self):
        return self._text

    def close(self):
        self.closed = True

    def release(self):
        self.closed = True


class FakeSession:
    def __init__(self, response):
        self._response = response
        self.calls = []
        self.closed = False

    async def post(self, url, params=None, json=None, headers=None):
        self.calls.append(
            {"url": url, "params": params, "json": json, "headers": headers}
        )
        return self._response

    async def close(self):
        self.closed = True


class FakeEnv:
    def __init__(self):
        self.errors = []

    def log_error(self, message, **_kwargs):
        self.errors.append(message)

    def log_info(self, *_args, **_kwargs):
        pass


class FakeConfig:
    def __init__(self, **overrides):
        self.params = BreezeTTSParams(
            api_key="brz_test", voice_id="voc_test", **overrides
        )
        self.dump = False
        self.dump_path = "/tmp"


def make_client(response):
    client = BreezeTTSClient(FakeConfig(), FakeEnv())
    client._session = FakeSession(response)
    return client


async def collect(client, text="你好"):
    return [item async for item in client.get(text, "req-1")]


def test_streams_chunks_then_end():
    client = make_client(FakeResponse(chunks=[b"a" * 10, b"b" * 6]))
    events = asyncio.run(collect(client))

    assert [e[1] for e in events] == [
        TTS2HttpResponseEventType.RESPONSE,
        TTS2HttpResponseEventType.RESPONSE,
        TTS2HttpResponseEventType.END,
    ]
    assert events[0][0] == b"a" * 10
    assert events[2][0] is None


def test_request_matches_breeze_contract():
    client = make_client(FakeResponse(chunks=[b"x"]))
    asyncio.run(collect(client, "念一句"))

    call = client._session.calls[0]
    assert call["url"].endswith("/v1/text-to-speech/voc_test/stream")
    assert call["params"] == {"output_format": "pcm"}
    assert call["headers"] == {"xi-api-key": "brz_test"}
    assert call["json"] == {"text": "念一句", "language_code": "zh"}


def test_instructions_and_speed_are_forwarded():
    client = BreezeTTSClient(
        FakeConfig(instructions="轻柔一点", speed=0.9), FakeEnv()
    )
    client._session = FakeSession(FakeResponse(chunks=[b"x"]))
    asyncio.run(collect(client))

    body = client._session.calls[0]["json"]
    assert body["instructions"] == "轻柔一点"
    assert body["voice_settings"] == {"speed": 0.9}


def test_invalid_key_is_reported():
    client = make_client(FakeResponse(status=401, text="bad key"))
    events = asyncio.run(collect(client))

    assert len(events) == 1
    payload, event = events[0]
    assert event is TTS2HttpResponseEventType.INVALID_KEY_ERROR
    assert payload == b"bad key"


def test_cancel_yields_flush():
    client = make_client(FakeResponse(chunks=[b"a" * 4, b"b" * 4]))
    stream = client.get("长文本", "req-2")

    async def drive():
        first = await stream.__anext__()
        assert first[1] is TTS2HttpResponseEventType.RESPONSE
        await client.cancel()
        rest = [item async for item in stream]
        return rest

    rest = asyncio.run(drive())
    assert rest[-1][1] is TTS2HttpResponseEventType.FLUSH


def test_empty_text_finishes_immediately():
    client = make_client(FakeResponse(chunks=[]))
    events = asyncio.run(collect(client, "   "))
    assert events == [(None, TTS2HttpResponseEventType.END)]


def test_sample_rate_matches_pcm_layout():
    assert SAMPLE_RATE == 24000
    assert BYTES_PER_SAMPLE == 2
