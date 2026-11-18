"""Basic Prism wire contract, interruption filtering, and owned cleanup."""

import asyncio
import base64

import pytest

from prism_test_protocol.audio import AudioBufferOverflow, InputAudio
from prism_test_protocol.config import INPUT_FRAME_BYTES, RimeRealtimeConfig
from prism_test_protocol.connection import PrismError


@pytest.mark.asyncio
async def test_initialization_gates_audio_and_authenticates(peer, connection):
    client, _ = connection
    with pytest.raises(PrismError, match="not initialized"):
        await client.send_audio(b"\0\0")
    await client.start()
    assert peer.authorizations == ["Bearer test-only"]
    setup = await peer.request("session.update")
    assert setup["session"] == {"voice": "aurora", "instructions": "", "tools": []}
    await client.send_audio(b"\1\0")
    audio = await peer.request("input_audio_buffer.append")
    assert base64.b64decode(audio["audio"]) == b"\1\0"


@pytest.mark.asyncio
async def test_greeting_waits_for_readiness_and_new_speech_supersedes_it(
    peer, connection
):
    client, _ = connection
    await client.start()
    await peer.request("session.update")
    await client.trigger("Hello")
    assert peer.requests.empty()
    await client._dispatch({"type": "input_audio_buffer.speech_started"})
    await client._dispatch({"type": "prism.typed_input.ready"})
    await client._dispatch({"type": "response.created", "response": {"id": "reply"}})
    await client._dispatch({"type": "prism.typed_input.ready"})
    await client._dispatch({"type": "prism.typed_input.ready"})
    assert not client._triggers
    await client.trigger("New greeting")
    request = await peer.request("response.create")
    assert request["response"]["prism_input_text"] == "New greeting"
    assert request["response"]["metadata"]["prism_cause"] == "user_text"


@pytest.mark.asyncio
async def test_busy_greeting_does_not_reappear_after_caller_speech(peer, connection):
    client, _ = connection
    await client.start()
    await client._dispatch({"type": "prism.typed_input.ready"})
    await client.trigger("Old greeting")
    request = await peer.request("response.create")
    await client._dispatch({"type": "input_audio_buffer.speech_started"})
    await client._dispatch(
        {
            "type": "error",
            "error": {
                "code": "typed_turn_busy",
                "scope": "request",
                "event_id": request["event_id"],
            },
        }
    )
    assert not client._triggers
    assert client._trigger_inflight is None


@pytest.mark.asyncio
async def test_late_output_is_dropped_after_interruption(peer, connection):
    client, events = connection
    await client.start()
    await client._dispatch({"type": "response.created", "response": {"id": "old"}})
    await client._dispatch({"type": "input_audio_buffer.speech_started"})
    while not events.empty():
        events.get_nowait()
    await client._dispatch(
        {"type": "response.audio.delta", "response_id": "old", "delta": "AAA="}
    )
    await client._dispatch(
        {"type": "response.text.done", "response_id": "old", "text": "Old answer"}
    )
    assert events.empty()
    await client._dispatch({"type": "response.created", "response": {"id": "new"}})
    await client._dispatch(
        {"type": "response.text.done", "response_id": "new", "text": "New answer"}
    )
    assert (await events.get())["type"] == "response.created"
    assert (await events.get())["text"] == "New answer"


@pytest.mark.asyncio
async def test_auth_refusal_is_not_retried(peer, connection):
    peer.refuse_status = 401
    client, _ = connection
    client.config.startup_retries = 2
    with pytest.raises(PrismError) as failure:
        await client.start()
    assert failure.value.code == "401"
    assert len(peer.authorizations) == 1
    assert client._session.closed and client._receiver is None


@pytest.mark.asyncio
async def test_missing_initial_ack_times_out_and_closes(peer, connection):
    peer.auto_initialize = False
    client, _ = connection
    client.config.startup_timeout = 0.05
    with pytest.raises(PrismError) as failure:
        await client.start()
    assert failure.value.code == "startup_timeout"
    assert client._session.closed and client._receiver is None


@pytest.mark.asyncio
async def test_cancelled_startup_joins_receiver(peer, connection):
    peer.auto_initialize = False
    client, _ = connection
    task = asyncio.create_task(client.start())
    await peer.request("session.update")
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert client._receiver is None and client._session.closed


@pytest.mark.asyncio
async def test_transient_refusal_has_bounded_retries(peer, connection):
    peer.refuse_status = 503
    client, _ = connection
    client.config.startup_retries = 1
    with pytest.raises(PrismError):
        await client.start()
    assert len(peer.authorizations) == 2
    assert client._session.closed


@pytest.mark.asyncio
async def test_disconnect_reports_terminal_error_without_reconnection(peer, connection):
    client, events = connection
    await client.start()
    await (await peer.connected).close()
    while True:
        event = await asyncio.wait_for(events.get(), 2)
        if event["type"] == "error":
            break
    assert event["error"]["scope"] == "session"
    await client._receiver
    assert not client.ready and len(peer.authorizations) == 1


@pytest.mark.asyncio
async def test_malformed_event_fails_safely(peer, connection):
    client, events = connection
    await client.start()
    await (await peer.connected).send_str("[]")
    while True:
        event = await asyncio.wait_for(events.get(), 2)
        if event["type"] == "error":
            break
    assert event["error"]["code"] == "protocol_error"
    await client._receiver
    assert client._session.closed


@pytest.mark.asyncio
async def test_write_deadline_includes_waiting_for_writer_lock(peer, connection):
    client, _ = connection
    await client.start()
    client.config.startup_timeout = 0.02
    async with client._write_lock:
        with pytest.raises(PrismError) as failure:
            await asyncio.wait_for(client.send({"type": "response.create"}), 0.5)
        assert failure.value.fatal


def test_pcm_buffer_keeps_whole_samples_pads_silence_and_refuses_backlog():
    audio = InputAudio()
    audio.append(b"\1\0")
    frame = audio.frame()
    assert len(frame) == INPUT_FRAME_BYTES and frame[:2] == b"\1\0"
    assert not any(frame[2:])
    with pytest.raises(ValueError):
        audio.append(b"\1")
    with pytest.raises(AudioBufferOverflow):
        audio.append(b"\0" * 32002)
    assert not audio.buffer


@pytest.mark.parametrize(
    "properties",
    [
        {"endpoint": "ws://example.com"},
        {"endpoint": "wss://key@example.com"},
        {"manual_turns": True},
        {"speed": 1.1},
    ],
)
def test_configuration_refuses_unsafe_urls_and_unsupported_features(properties):
    with pytest.raises(ValueError):
        RimeRealtimeConfig(**properties)


def test_config_redacts_credentials():
    config = RimeRealtimeConfig(api_key="test-only")
    assert "test-only" not in config.model_dump_json()
