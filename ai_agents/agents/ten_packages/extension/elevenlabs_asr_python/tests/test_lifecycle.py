"""Exercise the real websocket adapter through TEN audio/data boundaries."""

import asyncio
import base64
import json
from contextlib import asynccontextmanager
from urllib.parse import parse_qs, urlsplit

import pytest
from ten_runtime import AudioFrame, Data
from ten_ai_base.message import ModuleErrorCode
from websockets.exceptions import ConnectionClosedError, ConnectionClosedOK
from websockets.frames import Close
from websockets.protocol import State

from .. import recognition
from ..extension import ElevenLabsASRExtension
from ..reconnect_manager import ReconnectManager


async def until(predicate):
    async def poll():
        while not predicate():
            await asyncio.sleep(0.001)

    await asyncio.wait_for(poll(), 2)


def data(name, payload=None):
    result = Data.create(name)
    result.set_property_from_json(None, json.dumps(payload or {}))
    return result


def audio(milliseconds=100, session="100", value=1):
    frame = AudioFrame.create("pcm_frame")
    frame.set_sample_rate(16000)
    frame.set_bytes_per_sample(2)
    frame.set_number_of_channels(1)
    frame.set_samples_per_channel(milliseconds * 16)
    frame.alloc_buf(milliseconds * 32)
    buf = frame.lock_buf()
    buf[:] = bytes([value]) * len(buf)
    frame.unlock_buf(buf)
    frame.set_property_from_json(
        "metadata", json.dumps({"session_id": session, "custom": session})
    )
    return frame


class Env:
    def __init__(self, timestamps=False):
        self.events = []
        self.timestamps = timestamps
        self.logs = []

    async def get_property_bool(self, name):
        return False, None

    async def get_property_to_json(self, name):
        return (
            json.dumps(
                {
                    "language": "en-US",
                    "params": {
                        "api_key": "private-api-key",
                        "token": "private-token",
                        "ws_url": "wss://example.test/asr?token=private-url-token",
                        "include_timestamps": self.timestamps,
                        "finalize_timeout": 0.05,
                    },
                }
            ),
            None,
        )

    async def send_data(self, message):
        await asyncio.sleep(0)
        payload, error = message.get_property_to_json()
        assert error is None
        self.events.append((message.get_name(), json.loads(payload)))

    def output(self, name):
        return [payload for kind, payload in self.events if kind == name]

    def log_info(self, message, **kwargs):
        self.logs.append(str(message))

    log_debug = log_info
    log_warn = log_info
    log_error = log_info


class Socket:
    def __init__(self, number, startup=None):
        self.state = State.OPEN
        self.incoming = asyncio.Queue()
        self.sent = []
        self.send_gate = None
        self.send_entered = asyncio.Event()
        self.number = number
        self.incoming.put_nowait(
            startup
            or {
                "message_type": "session_started",
                "session_id": f"vendor-{number}",
            }
        )

    def __aiter__(self):
        return self

    async def __anext__(self):
        item = await self.incoming.get()
        if isinstance(item, Exception):
            raise item
        return json.dumps(item)

    def event(self, kind, **payload):
        self.incoming.put_nowait({"message_type": kind, **payload})

    def remote_close(self, normal=True, code=1000, reason="idle timeout"):
        self.state = State.CLOSED
        exc = ConnectionClosedOK if normal else ConnectionClosedError
        self.incoming.put_nowait(
            exc(
                Close(code, reason),
                Close(code, reason) if normal else None,
                True if normal else None,
            )
        )

    async def send(self, payload):
        self.send_entered.set()
        if self.send_gate is not None:
            await self.send_gate.wait()
        if self.state != State.OPEN:
            raise ConnectionClosedError(Close(1006, "lost"), None)
        self.sent.append(json.loads(payload))

    async def close(self):
        self.state = State.CLOSED


class Server:
    def __init__(self):
        self.sockets = []
        self.urls = []
        self.next_gate = None
        self.startup = None

    async def connect(self, url, **kwargs):
        self.urls.append(url)
        gate = self.next_gate
        self.next_gate = None
        if gate is not None:
            await gate.wait()
        socket = Socket(len(self.sockets), self.startup)
        self.startup = None
        self.sockets.append(socket)
        return socket


@asynccontextmanager
async def running(monkeypatch, *, timestamps=False):
    server = Server()
    monkeypatch.setattr(recognition.websockets, "connect", server.connect)
    env = Env(timestamps)
    extension = ElevenLabsASRExtension("elevenlabs")
    await extension.on_init(env)
    extension.reconnect_manager = ReconnectManager(
        base_delay=0.01, max_delay=0.01
    )
    try:
        yield extension, env, server
    finally:
        await extension.on_stop(env)
        await extension.on_deinit(env)


@pytest.mark.parametrize("explicit", [False, True])
def test_handshake_drains_audio_without_another_frame(monkeypatch, explicit):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            gate = asyncio.Event()
            server.next_gate = gate
            connecting = asyncio.create_task(
                ext.on_data(env, data("trigger_connect"))
                if explicit
                else ext.start_connection()
            )
            await until(lambda: len(server.urls) == 1)
            await ext.on_audio_frame(env, audio(session="100"))
            await ext.on_audio_frame(env, audio(session="200", value=2))
            gate.set()
            await asyncio.wait_for(connecting, 2)
            socket = server.sockets[0]
            assert len(socket.sent) == 2
            assert (
                base64.b64decode(socket.sent[0]["audio_base_64"])
                == bytes([1]) * 3200
            )
            assert (
                base64.b64decode(socket.sent[1]["audio_base_64"])
                == bytes([2]) * 3200
            )
            socket.event("committed_transcript", text="hello")
            await until(lambda: len(env.output("asr_result")) == 1)
            assert (
                env.output("asr_result")[0]["metadata"]["session_id"] == "200"
            )
            await ext.on_data(
                env,
                data(
                    "asr_finalize",
                    {"finalize_id": "done", "metadata": {"session_id": "200"}},
                ),
            )
            assert socket.sent[-1]["commit"] is True
            assert env.output("asr_finalize_end")[0]["finalize_id"] == "done"

    asyncio.run(run())


def test_concurrent_reconnect_and_trigger_use_one_handshake(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            await ext.on_audio_frame(env, audio())
            await until(lambda: len(server.sockets[0].sent) == 1)
            gate = asyncio.Event()
            server.next_gate = gate
            server.sockets[0].remote_close()
            await until(lambda: len(server.urls) == 2)
            trigger = asyncio.create_task(
                ext.on_data(env, data("trigger_connect"))
            )
            await asyncio.sleep(0.02)
            assert len(server.urls) == 2
            gate.set()
            await asyncio.wait_for(trigger, 2)
            await until(ext.is_connected)
            assert len(server.sockets) == 2
            assert env.output("error") == []

    asyncio.run(run())


@pytest.mark.parametrize("stop", [False, True])
def test_send_finishes_before_transport_is_closed(monkeypatch, stop):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            gate = asyncio.Event()
            socket.send_gate = gate
            sending = asyncio.create_task(ext.send_audio(audio(), "100"))
            await socket.send_entered.wait()
            closing = asyncio.create_task(
                ext.on_stop(env) if stop else ext.stop_connection()
            )
            await asyncio.sleep(0.01)
            assert socket.state == State.OPEN
            gate.set()
            assert await sending
            await asyncio.wait_for(closing, 2)
            assert socket.state == State.CLOSED
            assert len(socket.sent) == 1
            assert not ext.is_connected()

    asyncio.run(run())


def test_stop_during_reconnect_handshake_releases_socket(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            await ext.on_audio_frame(env, audio())
            await until(lambda: server.sockets[0].sent)
            server.next_gate = asyncio.Event()
            server.sockets[0].remote_close()
            await until(lambda: len(server.urls) == 2)
            await asyncio.wait_for(ext.on_stop(env), 1)
            await asyncio.sleep(0.02)
            assert len(server.urls) == 2
            assert not ext.is_connected()

    asyncio.run(run())


@pytest.mark.parametrize("enrichment", [False, True])
def test_replacement_keeps_committed_final_and_original_context(
    monkeypatch, enrichment
):
    async def run():
        async with running(monkeypatch, timestamps=True) as (ext, env, server):
            await ext.start_connection()
            first = server.sockets[0]
            await ext.on_audio_frame(env, audio(500, session="100"))
            await until(lambda: first.sent)
            first.event("committed_transcript", text="old")
            # Reader handles the commit before the close, without waiting for
            # its enrichment timeout.
            if enrichment:
                first.event(
                    "committed_transcript_with_timestamps",
                    text="old",
                    words=[{"text": "old", "start": 0.1, "end": 0.4}],
                )
            first.remote_close()
            await until(lambda: len(server.sockets) == 2)
            await ext.on_audio_frame(env, audio(100, session="200"))
            second = server.sockets[1]
            await until(lambda: second.sent)
            second.event("committed_transcript", text="new")
            await until(lambda: len(env.output("asr_result")) == 2)
            results = env.output("asr_result")
            assert [r["text"] for r in results] == ["old", "new"]
            assert [r["metadata"]["session_id"] for r in results] == [
                "100",
                "200",
            ]
            assert (
                results[0]["metadata"]["asr_info"]["vendor_session_id"]
                == "vendor-0"
            )
            if enrichment:
                assert results[0]["words"][0]["start_ms"] == 100
            else:
                assert results[0]["duration_ms"] == 500

    asyncio.run(run())


def test_nonfatal_then_fatal_error_is_not_suppressed(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            socket.event("commit_throttled", error="slow down")
            socket.event("quota_exceeded", error="quota exhausted")
            await until(lambda: len(env.output("error")) == 2)
            assert [r["vendor_info"]["code"] for r in env.output("error")] == [
                "commit_throttled",
                "quota_exceeded",
            ]
            assert socket.state == State.CLOSED
            assert not ext.is_connected()
            assert not await ext.send_audio(audio(), "100")
            await asyncio.sleep(0.03)
            assert len(server.urls) == 1
            closed = [
                r
                for r in env.output("connection_status_changed")
                if r["current"] == "disconnected"
            ]
            assert len(closed) == 1
            assert str(closed[0]["code"]) == ModuleErrorCode.FATAL_ERROR.value

    asyncio.run(run())


def test_auth_message_before_handshake_reports_once(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            server.startup = {"message_type": "auth_error", "error": "denied"}
            await ext.start_connection()
            assert len(env.output("error")) == 1
            assert env.output("error")[0]["vendor_info"]["code"] == "auth_error"
            assert not ext.is_connected()
            assert server.sockets[0].state == State.CLOSED
            assert [
                r["current"] for r in env.output("connection_status_changed")
            ] == ["connecting", "disconnected"]

    asyncio.run(run())


@pytest.mark.parametrize("normal", [False, True])
def test_close_exception_type_and_framework_severity(monkeypatch, normal):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            server.sockets[0].remote_close(normal=normal)
            await until(
                lambda: any(
                    r["current"] == "disconnected"
                    for r in env.output("connection_status_changed")
                )
            )
            assert len(env.output("error")) == (0 if normal else 1)
            closed = env.output("connection_status_changed")[-1]
            assert str(closed["code"]) == (
                "0" if normal else ModuleErrorCode.NON_FATAL_ERROR.value
            )
            assert closed["vendor_info"]["code"] == "1000"
            await asyncio.sleep(0.03)
            assert len(server.urls) == 1  # No audio: defer reconnect.
            await ext.on_audio_frame(env, audio())
            await until(
                lambda: len(server.sockets) == 2 and server.sockets[1].sent
            )

    asyncio.run(run())


@pytest.mark.parametrize("late_first", [False, True])
def test_repeated_text_with_missing_or_late_enrichment(monkeypatch, late_first):
    async def run():
        async with running(monkeypatch, timestamps=True) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            await ext.on_audio_frame(env, audio(500))
            await until(lambda: len(socket.sent) == 1)
            socket.event("committed_transcript", text="hello")
            await until(lambda: len(env.output("asr_result")) == 1)
            await ext.on_audio_frame(env, audio(1000))
            await until(lambda: len(socket.sent) == 2)
            socket.event("committed_transcript", text="hello")
            if late_first:
                socket.event(
                    "committed_transcript_with_timestamps",
                    text="hello",
                    words=[{"text": "hello", "start": 0, "end": 0.4}],
                )
            socket.event(
                "committed_transcript_with_timestamps",
                text="hello",
                words=[{"text": "hello", "start": 0.8, "end": 1.2}],
            )
            await until(lambda: len(env.output("asr_result")) == 2)
            await asyncio.sleep(0.3)
            results = env.output("asr_result")
            assert len(results) == 2
            assert results[0]["words"] == []
            assert results[1]["words"][0]["start_ms"] == 800
            assert results[1]["duration_ms"] == 400

    asyncio.run(run())


def test_connection_status_redacts_all_credentials(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            server.sockets[0].remote_close()
            await until(
                lambda: len(env.output("connection_status_changed")) == 3
            )
            serialized = json.dumps(
                env.output("connection_status_changed")
            ) + str(env.logs)
            for secret in (
                "private-api-key",
                "private-token",
                "private-url-token",
            ):
                assert secret not in serialized
            query = parse_qs(urlsplit(server.urls[0]).query)
            assert query["language_code"] == ["en"]

    asyncio.run(run())


def test_pending_final_metadata_survives_newer_audio(monkeypatch):
    async def run():
        async with running(monkeypatch, timestamps=True) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            await ext.on_audio_frame(env, audio(500, session="100"))
            await until(lambda: len(socket.sent) == 1)
            socket.event("committed_transcript", text="first")
            socket.event("warning", warning="commit processed")
            await until(
                lambda: any("commit processed" in log for log in env.logs)
            )
            await ext.on_audio_frame(env, audio(200, session="200"))
            await until(lambda: len(socket.sent) == 2)
            await until(lambda: len(env.output("asr_result")) == 1)
            result = env.output("asr_result")[0]
            assert result["metadata"]["session_id"] == "100"
            assert result["metadata"]["custom"] == "100"
            assert result["duration_ms"] == 500
            socket.event("committed_transcript", text="second")
            await until(lambda: len(env.output("asr_result")) == 2)
            second = env.output("asr_result")[1]
            assert second["metadata"]["session_id"] == "200"
            assert (second["start_ms"], second["duration_ms"]) == (500, 200)

    asyncio.run(run())


def test_stop_cannot_cancel_committed_final_delivery(monkeypatch):
    async def run():
        async with running(monkeypatch, timestamps=True) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            await ext.on_audio_frame(env, audio(500))
            await until(lambda: socket.sent)
            entered = asyncio.Event()
            release = asyncio.Event()
            original_send = env.send_data

            async def yielding_output(message):
                if message.get_name() == "asr_result":
                    entered.set()
                    await release.wait()
                await original_send(message)

            env.send_data = yielding_output
            socket.event("committed_transcript", text="hello")
            socket.event(
                "committed_transcript_with_timestamps",
                text="hello",
                words=[{"text": "hello", "start": 0, "end": 0.4}],
            )
            await entered.wait()
            stopping = asyncio.create_task(ext.on_stop(env))
            await asyncio.sleep(0.01)
            release.set()
            await asyncio.wait_for(stopping, 2)
            assert len(env.output("asr_result")) == 1
            assert env.output("asr_result")[0]["text"] == "hello"

    asyncio.run(run())


def test_dumper_cannot_resume_against_a_replacement_client(monkeypatch):
    class Dumper:
        def __init__(self):
            self.entered = asyncio.Event()
            self.release = asyncio.Event()

        async def push_bytes(self, payload):
            self.entered.set()
            await self.release.wait()

        async def stop(self):
            pass

    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            old = server.sockets[0]
            dumper = Dumper()
            ext.audio_dumper = dumper
            sending = asyncio.create_task(ext.send_audio(audio(), "100"))
            await dumper.entered.wait()

            async def replace():
                await ext.stop_connection()
                await ext.start_connection()

            replacing = asyncio.create_task(replace())
            await asyncio.sleep(0.01)
            assert old.state == State.OPEN
            assert len(server.urls) == 1
            dumper.release.set()
            assert await sending
            await asyncio.wait_for(replacing, 2)
            assert len(old.sent) == 1
            assert server.sockets[1].sent == []

    asyncio.run(run())


def test_finalize_and_stop_share_the_client_operation_boundary(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            await ext.start_connection()
            socket = server.sockets[0]
            await ext.on_audio_frame(env, audio())
            await until(lambda: len(socket.sent) == 1)
            socket.send_entered.clear()
            socket.send_gate = asyncio.Event()
            finalizing = asyncio.create_task(
                ext.on_data(
                    env,
                    data(
                        "asr_finalize",
                        {
                            "finalize_id": "final-1",
                            "metadata": {"session_id": "100"},
                        },
                    ),
                )
            )
            await socket.send_entered.wait()
            stopping = asyncio.create_task(ext.on_stop(env))
            await asyncio.sleep(0.01)
            assert socket.state == State.OPEN
            socket.send_gate.set()
            await until(lambda: len(socket.sent) == 2)
            socket.event("committed_transcript", text="hello")
            await asyncio.wait_for(finalizing, 2)
            await asyncio.wait_for(stopping, 2)
            assert socket.sent[-1]["commit"] is True
            assert socket.state == State.CLOSED
            assert env.output("asr_result")[0]["final"] is True
            assert len(env.output("asr_finalize_end")) == 1

    asyncio.run(run())


def test_failed_handshake_cannot_disconnect_concurrent_trigger(monkeypatch):
    async def run():
        async with running(monkeypatch) as (ext, env, server):
            # Both public connection paths run while the first startup is
            # waiting. Only the first receives an explicit transient error.
            server.startup = {"message_type": "rate_limited", "error": "busy"}
            first = asyncio.create_task(ext.start_connection())
            second = asyncio.create_task(
                ext.on_data(env, data("trigger_connect"))
            )
            await asyncio.wait_for(asyncio.gather(first, second), 2)
            await until(ext.is_connected)
            await asyncio.sleep(0.03)
            assert len(server.sockets) == 2
            assert server.sockets[0].state == State.CLOSED
            assert server.sockets[1].state == State.OPEN
            assert len(env.output("error")) == 1
            states = env.output("connection_status_changed")
            assert states[-1]["current"] == "connected"
            connected_index = max(
                i for i, r in enumerate(states) if r["current"] == "connected"
            )
            assert not any(
                r["current"] == "disconnected"
                for r in states[connected_index + 1 :]
            )

    asyncio.run(run())
