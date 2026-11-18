"""TEN interface tests use the real base helpers and a native-runtime double."""

import asyncio
import base64
import json

import pytest


def test_package_entrypoint_registers_and_creates_the_addon(ten_modules):
    assert set(ten_modules.registered) == {"rime_mllm_python"}
    addon = ten_modules.registered["rime_mllm_python"]()
    environment = ten_modules.environment({})
    addon.on_create_instance(environment, "v2v", "context")
    instance, context = environment.created
    assert isinstance(instance, ten_modules.extension.RimeRealtimeExtension)
    assert context == "context"


async def next_data(environment, name):
    while True:
        message = await asyncio.wait_for(environment.data.get(), 2)
        if message.get_name() == name:
            return message.payload


@pytest.mark.asyncio
async def test_pcm_captions_late_transcripts_and_interrupted_audio(peer, ten_modules):
    environment = ten_modules.environment(
        {"endpoint": peer.url, "api_key": "test-only"}
    )
    extension = ten_modules.extension.RimeRealtimeExtension("v2v")
    await extension.on_init(environment)
    try:
        await extension.on_start(environment)
        await next_data(environment, "mllm_server_session_ready")
        await peer.send({"type": "response.created", "response": {"id": "reply"}})
        await peer.send(
            {
                "type": "response.text.delta",
                "response_id": "reply",
                "delta": "Hello",
            }
        )
        audio = base64.b64encode(b"\1\0" * 24).decode()
        await peer.send(
            {
                "type": "response.audio.delta",
                "response_id": "reply",
                "delta": audio,
            }
        )
        frame = await asyncio.wait_for(environment.audio.get(), 2)
        assert frame.get_sample_rate() == 24000
        assert frame.get_number_of_channels() == 1
        assert frame.get_bytes_per_sample() == 2
        assert bytes(frame.get_buf()) == b"\1\0" * 24
        assert (
            environment.data.empty()
        ), "Partial captions are outside the basic interface"
        await peer.send(
            {
                "type": "response.text.done",
                "response_id": "reply",
                "text": "Hello there",
            }
        )
        assert (await next_data(environment, "mllm_server_output_transcript"))["final"]
        await peer.send(
            {
                "type": "response.done",
                "response": {"id": "reply", "status": "completed"},
            }
        )
        await peer.send(
            {
                "type": "conversation.item.input_audio_transcription.completed",
                "transcript": "A late transcript",
            }
        )
        transcript = await next_data(environment, "mllm_server_input_transcript")
        assert transcript["content"] == "A late transcript" and transcript["final"]
        await peer.send({"type": "input_audio_buffer.speech_started"})
        await next_data(environment, "mllm_server_interrupted")
        await peer.send(
            {
                "type": "response.audio.delta",
                "response_id": "reply",
                "delta": audio,
            }
        )
        await peer.send({"type": "response.created", "response": {"id": "next"}})
        await peer.send(
            {
                "type": "response.audio.delta",
                "response_id": "next",
                "delta": base64.b64encode(b"\2\0").decode(),
            }
        )
        assert (
            bytes((await asyncio.wait_for(environment.audio.get(), 2)).get_buf())
            == b"\2\0"
        )
        assert environment.audio.empty()
        frame = ten_modules.message("pcm_frame", {"metadata": {"session_id": "123"}})
        frame.buffer = bytearray(b"\3\0" * 320)
        await extension.on_audio_frame(environment, frame)
        while True:
            request = await peer.request("input_audio_buffer.append")
            if base64.b64decode(request["audio"]) == bytes(frame.buffer):
                break
        frame.sample_rate = 24000
        await extension.on_audio_frame(environment, frame)
        error = await next_data(environment, "error")
        assert error["vendor_info"]["code"] == "invalid_audio"
        assert extension.is_connected()
        assert "test-only" not in "\n".join(environment.logs)
    finally:
        await extension.on_stop(environment)
    assert extension._audio_sender is None
    assert extension.connection._receiver is None
    assert not extension.is_connected()


@pytest.mark.asyncio
async def test_audio_backlog_is_fatal_and_clears_stale_input(peer, ten_modules):
    environment = ten_modules.environment(
        {"endpoint": peer.url, "api_key": "test-only"}
    )
    extension = ten_modules.extension.RimeRealtimeExtension("v2v")
    await extension.on_init(environment)
    await extension.on_start(environment)
    frame = ten_modules.message("pcm_frame")
    frame.buffer = bytearray(32002)
    await extension.on_audio_frame(environment, frame)
    failure = await next_data(environment, "error")
    assert failure["vendor_info"]["code"] == "audio_backlog"
    assert failure["code"] == -1000
    assert not extension.is_connected()
    assert not extension.input_audio.buffer
    assert extension._audio_sender is None
    await extension.on_stop(environment)


@pytest.mark.asyncio
@pytest.mark.parametrize("properties", [{}, {"endpoint": "ws://example.com"}])
async def test_invalid_configuration_and_missing_key_are_fatal(ten_modules, properties):
    environment = ten_modules.environment(properties)
    extension = ten_modules.extension.RimeRealtimeExtension("v2v")
    await extension.on_init(environment)
    await extension.on_start(environment)
    failure = await next_data(environment, "error")
    assert failure["code"] == -1000
    assert not extension.is_connected()
    assert extension._audio_sender is None
    await extension.on_stop(environment)


@pytest.mark.asyncio
async def test_unsupported_features_report_recoverable_errors(peer, ten_modules):
    environment = ten_modules.environment(
        {"endpoint": peer.url, "api_key": "test-only"}
    )
    extension = ten_modules.extension.RimeRealtimeExtension("v2v")
    await extension.on_init(environment)
    await extension.on_start(environment)
    try:
        await extension.send_client_register_tool(None)
        assert (await next_data(environment, "error"))["vendor_info"][
            "code"
        ] == "unsupported_tools"
        await extension.on_data(
            environment, ten_modules.message("mllm_client_set_message_context")
        )
        assert (await next_data(environment, "error"))["vendor_info"][
            "code"
        ] == "unsupported_history"
        assert extension.is_connected()
    finally:
        await extension.on_stop(environment)


@pytest.mark.asyncio
async def test_input_clock_recovers_from_a_short_write_stall(ten_modules, monkeypatch):
    from types import SimpleNamespace

    clock = SimpleNamespace(now=0.0)
    frames = []
    extension = ten_modules.extension.RimeRealtimeExtension("v2v")

    async def send_audio(frame):
        frames.append(frame)
        clock.now += 0.035 if len(frames) == 1 else 0.001
        if len(frames) == 10:
            extension.stopped = True

    async def sleep(seconds):
        assert 0 <= seconds <= 0.021
        clock.now += seconds

    extension.connection = SimpleNamespace(ready=True, send_audio=send_audio)
    monkeypatch.setattr(
        ten_modules.extension,
        "asyncio",
        SimpleNamespace(
            get_running_loop=lambda: SimpleNamespace(time=lambda: clock.now),
            sleep=sleep,
        ),
    )
    await extension._send_input()
    assert len(frames) == 10
    assert (
        clock.now <= 0.205
    ), "A short write stall must not permanently shift the PCM clock"
