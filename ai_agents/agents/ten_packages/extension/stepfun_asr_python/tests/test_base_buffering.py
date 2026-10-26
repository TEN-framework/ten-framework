import asyncio
import base64
import json
from contextlib import suppress
from types import SimpleNamespace

import pytest
from ten_ai_base.asr import AsyncASRBaseExtension

from stepfun_asr_python import recognition as recognition_module
from stepfun_asr_python.extension import StepFunASRExtension
from stepfun_asr_python.recognition import StepFunASRRecognition

from .test_recognition import Callback, config
from .test_extension import FakeWebSocket, _audio_frame, _extension


class CountingWebSocket:
    def __init__(self):
        self.open = True
        self.audio_bytes = 0
        self.events = []
        self.messages = asyncio.Queue()
        self.closed = asyncio.Event()
        self.audio_sent = asyncio.Event()

    async def send(self, raw):
        message = json.loads(raw)
        if message["type"] == "input_audio_buffer.append":
            self.audio_bytes += len(base64.b64decode(message["audio"]))
            self.audio_sent.set()
        else:
            self.events.append(message["type"])
        if message["type"] == "session.update":
            self.messages.put_nowait(json.dumps({"type": "session.updated"}))

    def __aiter__(self):
        return self

    async def __anext__(self):
        return await self.messages.get()

    async def close(self):
        self.open = False
        self.closed.set()


def test_audio_frame_handling_is_inherited():
    assert (
        StepFunASRExtension._handle_audio_frame
        is AsyncASRBaseExtension._handle_audio_frame
    )


@pytest.mark.asyncio
async def test_continuous_audio_does_not_require_final_to_keep_sending():
    callback = Callback()
    recognition = StepFunASRRecognition(config(), SimpleNamespace(), callback)
    socket = CountingWebSocket()
    recognition.websocket = socket
    recognition._started = True
    recognition._generation = 1
    chunk = b"\x01\x02" * 1600
    try:
        # More than the old 10 MiB limit, without any provider final.
        for _ in range(3300):
            assert await recognition.send_audio(chunk)
        assert socket.audio_bytes == len(chunk) * 3300
        assert callback.sent_bytes[-1] == socket.audio_bytes
        assert callback.errors == []
    finally:
        await recognition.shutdown()


@pytest.mark.asyncio
async def test_base_buffer_flushes_in_order_and_preserves_each_session():
    ext = _extension()
    callback = Callback()
    recognition = StepFunASRRecognition(ext.config, ext.ten_env, callback)
    ext.recognition = recognition
    first = b"\x01\x02" * 160
    second = b"\x03\x04" * 160
    try:
        await ext._handle_audio_frame(
            ext.ten_env, _audio_frame(first, {"session_id": "a"})
        )
        socket = FakeWebSocket()
        recognition.websocket = socket
        recognition._started = True
        recognition._generation = 1
        await ext._handle_audio_frame(
            ext.ten_env, _audio_frame(second, {"session_id": "b"})
        )
        await recognition.commit("final-b", 1.0, session_id="b")
        audio = [
            base64.b64decode(event["audio"])
            for event in socket.sent
            if event["type"] == "input_audio_buffer.append"
        ]
        assert audio == [first, second]
        for item_id in ("a", "b"):
            await recognition._handle_message(
                {
                    "type": "input_audio_buffer.committed",
                    "item_id": item_id,
                }
            )
            await recognition._handle_message(
                {
                    "type": "response.audio_transcription.completed",
                    "item_id": item_id,
                    "transcript": item_id,
                }
            )
        assert [
            (message["transcript"], segment.session_id)
            for message, segment in callback.messages
        ] == [("a", "a"), ("b", "b")]
    finally:
        await recognition.shutdown()


@pytest.mark.asyncio
async def test_open_flushes_base_buffer_without_another_audio_frame():
    ext = _extension()
    recognition = StepFunASRRecognition(ext.config, ext.ten_env, ext)
    ext.recognition = recognition
    audio = b"\x01\x02" * 160
    await ext._handle_audio_frame(
        ext.ten_env, _audio_frame(audio, {"session_id": "a"})
    )
    socket = CountingWebSocket()
    recognition.websocket = socket
    recognition._started = True
    consumer = asyncio.create_task(ext._audio_frame_consumer())
    try:
        await ext.on_open()
        await asyncio.wait_for(socket.audio_sent.wait(), 1)
        assert socket.audio_bytes == len(audio)
        assert ext.audio_timeline.total_user_audio_duration == 10
        assert ext.sent_buffer_length == len(audio)
    finally:
        consumer.cancel()
        with suppress(asyncio.CancelledError):
            await consumer
        await recognition.shutdown()


@pytest.mark.asyncio
async def test_reconnect_does_not_replay_previously_sent_audio(monkeypatch):
    sockets = []

    async def connect(_url, **_kwargs):
        socket = CountingWebSocket()
        sockets.append(socket)
        return socket

    monkeypatch.setattr(recognition_module.websockets, "connect", connect)
    recognition = StepFunASRRecognition(config(), SimpleNamespace(), Callback())
    try:
        await recognition.start()
        assert await recognition.send_audio(b"\x01\x02" * 160)
        await recognition.commit("before-disconnect", 1.0)
        await recognition.start()
        assert sockets[1].events == ["session.update"]
        assert sockets[1].audio_bytes == 0
        assert await recognition.send_audio(b"\x03\x04" * 160)
        assert sockets[0].audio_bytes == 320
        assert sockets[1].audio_bytes == 320
    finally:
        await recognition.shutdown()


@pytest.mark.asyncio
async def test_result_timestamps_continue_across_reconnect(monkeypatch):
    async def connect(_url, **_kwargs):
        return CountingWebSocket()

    monkeypatch.setattr(recognition_module.websockets, "connect", connect)
    callback = Callback()
    recognition = StepFunASRRecognition(config(), SimpleNamespace(), callback)
    try:
        for index in range(2):
            await recognition.start()
            assert await recognition.send_audio(b"\x01\x02" * 160)
            await recognition.commit(str(index), 1.0)
            await recognition._handle_message(
                {
                    "type": "input_audio_buffer.committed",
                    "item_id": str(index),
                }
            )
            await recognition._handle_message(
                {
                    "type": "response.audio_transcription.completed",
                    "item_id": str(index),
                    "transcript": "hello",
                    "words": [{"word": "hello", "start": 0, "end": 0.01}],
                }
            )
        assert [
            message["audio_offset_ms"] for message, _ in callback.messages
        ] == [0, 10]
        assert callback.sent_bytes == [320, 640]
    finally:
        await recognition.shutdown()
