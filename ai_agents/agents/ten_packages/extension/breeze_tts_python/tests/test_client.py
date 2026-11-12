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
    def __init__(self, status=200, chunks=(), text="", headers=None):
        self.status = status
        self.headers = headers if headers is not None else {"Content-Type": "audio/pcm"}
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

    def log_warn(self, *_args, **_kwargs):
        pass


class FakeConfig:
    def __init__(self, **overrides):
        self.params = BreezeTTSParams(
            api_key="brz_test", voice_id="voc_test", **overrides
        )
        self.dump = False
        self.dump_path = "/tmp"


def make_client(response, **params):
    config = FakeConfig()
    config.params = BreezeTTSParams(
        api_key="brz_test", voice_id="voc_test", **params
    )
    client = BreezeTTSClient(config, FakeEnv())
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


def test_resamples_to_the_configured_rate():
    chunks = [b"\x01\x00" * 24000]  # one second of 24 kHz mono PCM
    client = make_client(FakeResponse(chunks=chunks), sample_rate=16000)
    events = asyncio.run(collect(client))

    audio = b"".join(payload for payload, kind in events if kind is TTS2HttpResponseEventType.RESPONSE)
    # 1 s at 16 kHz, 16 bit mono
    assert abs(len(audio) - 32000) <= 64


def test_validation_error_is_recoverable():
    client = make_client(FakeResponse(status=422, text="Request validation failed."))
    events = asyncio.run(collect(client))

    assert events[-1][1] is TTS2HttpResponseEventType.ERROR


def test_credential_error_is_fatal():
    client = make_client(FakeResponse(status=401, text="bad key"))
    events = asyncio.run(collect(client))

    assert events[-1][1] is TTS2HttpResponseEventType.INVALID_KEY_ERROR


def test_non_audio_200_is_recoverable():
    client = make_client(
        FakeResponse(
            chunks=[b"<html>rate limited</html>"],
            headers={"Content-Type": "text/html"},
        )
    )
    events = asyncio.run(collect(client))

    assert events[-1][1] is TTS2HttpResponseEventType.ERROR
    assert not any(k is TTS2HttpResponseEventType.RESPONSE for _, k in events)


def test_truncated_final_sample_is_reported():
    client = make_client(FakeResponse(chunks=[b"\x01\x00\x02"]))
    events = asyncio.run(collect(client))

    audio = b"".join(p for p, k in events if k is TTS2HttpResponseEventType.RESPONSE)
    assert audio == b"\x01\x00"
    assert events[-1][1] is TTS2HttpResponseEventType.ERROR


def test_cancel_after_odd_chunk_does_not_emit_the_tail():
    async def scenario():
        client = make_client(FakeResponse(chunks=[b"\x01\x00\x03"]))
        stream = client.get("hello", "req-cancel")
        first = await stream.__anext__()
        await client.cancel()
        rest = [event async for event in stream]
        return first, rest

    first, rest = asyncio.run(scenario())

    assert first[1] is TTS2HttpResponseEventType.RESPONSE
    assert [k for _, k in rest] == [TTS2HttpResponseEventType.FLUSH]


def test_odd_chunk_keeps_samples_aligned():
    payload = b"\x01\x00\x02\x00\x03\x00"
    client = make_client(FakeResponse(chunks=[payload[:3], payload[3:]]))
    events = asyncio.run(collect(client))

    audio = b"".join(p for p, k in events if k is TTS2HttpResponseEventType.RESPONSE)
    assert audio == payload


class BlockingSession(FakeSession):
    """A session whose post() never resolves until it is cancelled."""

    def __init__(self):
        super().__init__(FakeResponse())
        self.started = asyncio.Event()

    async def post(self, url, params=None, json=None, headers=None):
        self.calls.append({"url": url})
        self.started.set()
        await asyncio.Event().wait()


def test_cancel_while_waiting_for_headers_yields_flush():
    async def scenario():
        client = make_client(FakeResponse())
        session = BlockingSession()
        client._session = session
        stream = client.get("hello", "req-headers")
        pending = asyncio.ensure_future(stream.__anext__())
        await asyncio.wait_for(session.started.wait(), 2)
        await client.cancel()
        event = await asyncio.wait_for(pending, 2)
        rest = [item async for item in stream]
        return event, rest

    event, rest = asyncio.run(scenario())

    assert event[1] is TTS2HttpResponseEventType.FLUSH
    assert rest == []


def test_cancel_during_retry_backoff_stops_further_requests():
    async def scenario():
        client = make_client(
            FakeResponse(status=503, headers={"Retry-After": "0.2"})
        )
        session = client._session
        stream = client.get("hello", "req-backoff")
        pending = asyncio.ensure_future(stream.__anext__())
        for _ in range(100):
            if session.calls:
                break
            await asyncio.sleep(0.01)
        await asyncio.sleep(0.05)
        await client.cancel()
        event = await asyncio.wait_for(pending, 2)
        rest = [item async for item in stream]
        return event, rest, len(session.calls)

    event, rest, calls = asyncio.run(scenario())

    assert event[1] is TTS2HttpResponseEventType.FLUSH
    assert rest == []
    assert calls == 1
