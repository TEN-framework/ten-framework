import asyncio
import struct

from unittest.mock import AsyncMock

import pytest
from ten_ai_base.struct import TTS2HttpResponseEventType

from gmicloud_tts2_python.client import GmicloudTTSClient
from tests.helpers import (
    FakeResponse,
    collect,
    make_opts,
    make_wav,
    split_events,
)


@pytest.mark.asyncio
async def test_sync_success_emits_response_then_end():
    pcm = struct.pack("<4h", 0, 100, -100, 32000)
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "success", "outcome": {"audio_url": "https://a/wav"}}))
    client._client.get = AsyncMock(return_value=FakeResponse(
        content=make_wav(pcm, 24000, 1)))
    audio, kinds = split_events(await collect(client, "hello"))
    assert audio == pcm
    assert kinds[-1] == TTS2HttpResponseEventType.END
    assert all(k == TTS2HttpResponseEventType.RESPONSE for k in kinds[:-1])
    await client.clean()


@pytest.mark.asyncio
async def test_queued_polls_until_success_and_sends_auth():
    pcm = struct.pack("<2h", 7, -7)
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "queued", "request_id": "r1"}))
    client._client.get = AsyncMock(side_effect=[
        FakeResponse(json_data={"status": "queued", "request_id": "r1"}),
        FakeResponse(json_data={"status": "success", "request_id": "r1",
                                "outcome": {"audio_url": "https://a/wav"}}),
        FakeResponse(content=make_wav(pcm, 24000, 1)),
    ])
    audio, kinds = split_events(await collect(client, "hello"))
    assert audio == pcm
    # Regression: the poll GET must carry the Bearer header (the queue
    # answers 401 to unauthenticated polls).
    poll_call = client._client.get.call_args_list[0]
    assert poll_call.kwargs["headers"]["Authorization"].startswith("Bearer ")
    await client.clean()


@pytest.mark.asyncio
async def test_poll_4xx_yields_error_event():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "queued", "request_id": "r1"}))
    client._client.get = AsyncMock(return_value=FakeResponse(status_code=404))
    audio, kinds = split_events(await collect(client, "hello"))
    assert audio == b""
    assert kinds == [TTS2HttpResponseEventType.ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_non_wav_audio_yields_error_event():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "success", "outcome": {"audio_url": "https://a/mp3"}}))
    client._client.get = AsyncMock(return_value=FakeResponse(content=b"ID3junk"))
    _, kinds = split_events(await collect(client, "hello"))
    assert kinds == [TTS2HttpResponseEventType.ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_long_text_is_chunked_and_concatenated():
    pcm = struct.pack("<2h", 1, -1)
    client = GmicloudTTSClient(make_opts(text_chunk_chars=60))
    calls = []

    async def fake_post(url, json=None, headers=None):
        calls.append(json["payload"]["text"])
        return FakeResponse(json_data={"status": "success",
                                      "outcome": {"audio_url": "https://a/wav"}})

    client._client.post = fake_post
    client._client.get = AsyncMock(return_value=FakeResponse(
        content=make_wav(pcm, 24000, 1)))
    text = " ".join(f"Number {i} is here." for i in range(30))
    audio, kinds = split_events(await collect(client, text))
    assert len(calls) > 1
    assert all(len(c) <= 60 for c in calls)
    assert audio == pcm * len(calls)
    assert kinds[-1] == TTS2HttpResponseEventType.END
    await client.clean()


@pytest.mark.asyncio
async def test_cancel_interrupts_blocked_post_promptly():
    client = GmicloudTTSClient(make_opts())
    gate = asyncio.Event()

    async def hanging_post(url, json=None, headers=None):
        await gate.wait()
        return FakeResponse()

    client._client.post = hanging_post
    task = asyncio.create_task(collect(client, "hello"))
    await asyncio.sleep(0.05)
    await client.cancel()
    events = await asyncio.wait_for(task, timeout=1.0)
    assert events == [(None, TTS2HttpResponseEventType.FLUSH)]
    assert not gate.is_set()  # the POST never completed
    await client.clean()


@pytest.mark.asyncio
async def test_cancel_interrupts_poll_sleep_promptly():
    client = GmicloudTTSClient(make_opts(poll_interval_sec=30.0))
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "queued", "request_id": "r1"}))
    task = asyncio.create_task(collect(client, "hello"))
    await asyncio.sleep(0.05)
    await client.cancel()
    events = await asyncio.wait_for(task, timeout=1.0)
    assert events == [(None, TTS2HttpResponseEventType.FLUSH)]
    await client.clean()


@pytest.mark.asyncio
async def test_cancel_interrupts_blocked_download_promptly():
    client = GmicloudTTSClient(make_opts())
    gate = asyncio.Event()

    async def hanging_get(url, **kwargs):
        await gate.wait()
        return FakeResponse()

    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "success", "outcome": {"audio_url": "https://a/wav"}}))
    client._client.get = hanging_get
    task = asyncio.create_task(collect(client, "hello"))
    await asyncio.sleep(0.05)
    await client.cancel()
    events = await asyncio.wait_for(task, timeout=1.0)
    assert events == [(None, TTS2HttpResponseEventType.FLUSH)]
    assert not gate.is_set()
    await client.clean()


@pytest.mark.asyncio
async def test_request_succeeds_after_cancellation():
    pcm = struct.pack("<2h", 9, -9)
    client = GmicloudTTSClient(make_opts())
    gate = asyncio.Event()

    async def hanging_post(url, json=None, headers=None):
        await gate.wait()
        return FakeResponse()

    client._client.post = hanging_post
    task = asyncio.create_task(collect(client, "first"))
    await asyncio.sleep(0.05)
    await client.cancel()
    events = await asyncio.wait_for(task, timeout=1.0)
    assert events == [(None, TTS2HttpResponseEventType.FLUSH)]

    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "success", "outcome": {"audio_url": "https://a/wav"}}))
    client._client.get = AsyncMock(return_value=FakeResponse(
        content=make_wav(pcm, 24000, 1)))
    audio, kinds = split_events(await collect(client, "second"))
    assert audio == pcm and kinds[-1] == TTS2HttpResponseEventType.END
    await client.clean()


@pytest.mark.asyncio
async def test_request_succeeds_after_recoverable_error():
    pcm = struct.pack("<2h", 5, -5)
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(side_effect=[
        FakeResponse(status_code=503),
        FakeResponse(json_data={"status": "success",
                                "outcome": {"audio_url": "https://a/wav"}}),
    ])
    client._client.get = AsyncMock(return_value=FakeResponse(
        content=make_wav(pcm, 24000, 1)))
    _, kinds1 = split_events(await collect(client, "first"))
    audio, kinds2 = split_events(await collect(client, "second"))
    assert kinds1 == [TTS2HttpResponseEventType.ERROR]
    assert audio == pcm and kinds2[-1] == TTS2HttpResponseEventType.END
    await client.clean()


@pytest.mark.asyncio
async def test_cancel_mid_request_yields_flush():
    client = GmicloudTTSClient(make_opts())
    started = asyncio.Event()

    async def fake_post(url, json=None, headers=None):
        started.set()
        await asyncio.sleep(0.05)
        return FakeResponse(json_data={"status": "success",
                                      "outcome": {"audio_url": "https://a/wav"}})

    client._client.post = fake_post
    task = asyncio.create_task(collect(client, "hello"))
    await started.wait()
    await client.cancel()
    events = await task
    assert events == [(None, TTS2HttpResponseEventType.FLUSH)]
    await client.clean()
