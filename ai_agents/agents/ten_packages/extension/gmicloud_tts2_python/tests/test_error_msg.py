from unittest.mock import AsyncMock

import pytest
from ten_ai_base.struct import TTS2HttpResponseEventType

from gmicloud_tts2_python.client import GmicloudTTSClient
from tests.helpers import FakeResponse, collect, make_opts, split_events


@pytest.mark.asyncio
async def test_auth_failure_maps_to_invalid_key_event():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(status_code=401))
    _, kinds = split_events(await collect(client, "hello"))
    assert kinds == [TTS2HttpResponseEventType.INVALID_KEY_ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_forbidden_maps_to_invalid_key_event():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(status_code=403))
    _, kinds = split_events(await collect(client, "hello"))
    assert kinds == [TTS2HttpResponseEventType.INVALID_KEY_ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_server_error_yields_error_event():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(status_code=503))
    _, kinds = split_events(await collect(client, "hello"))
    assert kinds == [TTS2HttpResponseEventType.ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_error_event_carries_message_bytes():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(status_code=500))
    events = await collect(client, "hello")
    payload, kind = events[0]
    assert kind == TTS2HttpResponseEventType.ERROR
    assert b"HTTP 500" in payload
    await client.clean()


@pytest.mark.asyncio
async def test_download_403_is_not_invalid_key():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        json_data={"status": "success", "outcome": {"audio_url": "https://a/wav"}}))
    client._client.get = AsyncMock(return_value=FakeResponse(status_code=403))
    _, kinds = split_events(await collect(client, "hello"))
    assert kinds == [TTS2HttpResponseEventType.ERROR]
    await client.clean()


@pytest.mark.asyncio
async def test_error_event_carries_response_body():
    client = GmicloudTTSClient(make_opts())
    client._client.post = AsyncMock(return_value=FakeResponse(
        status_code=500, json_data={"error": "upstream detail"}))
    events = await collect(client, "hello")
    payload, _ = events[0]
    assert b"upstream detail" in payload
    await client.clean()
