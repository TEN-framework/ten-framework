#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Native TEN routes -> production hooks/sender/pacer, without network calls.

The session-scoped FakeApp fixture is shared with the other runtime tests.
Only the external session and clock are replaced. Hook spies delegate to the
production methods and acknowledge completion across the two asyncio loops.
"""

import asyncio
import importlib
import json
import struct

import pytest
from ten_runtime import (
    AsyncExtensionTester,
    AudioFrame,
    AudioFrameDataFmt,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
)


# Import through the normal addon namespace, not the pure tests' stub package.
# The decorator registers the existing addon once; no test addon is introduced.
extension = importlib.import_module(
    "ten_packages.extension.spatialreal_avatar_python.extension"
)

RATE = 24000
META_A = {"session_id": "synthetic-session", "turn_id": 1}
META_B = {"session_id": "synthetic-session", "turn_id": 2}
PCM_A = struct.pack("<h", 1001) * 2400
PCM_B = struct.pack("<h", 2002) * 2400


class Clock:
    def __init__(self):
        self.value = 1000.0
        self.waits = []

    def now(self):
        return self.value

    async def sleep(self, seconds):
        self.waits.append(seconds)
        try:
            await asyncio.Event().wait()
        finally:
            self.waits.remove(seconds)


class RecordingSession:
    instances = []

    def __init__(self, _settings, **callbacks):
        self.callbacks = callbacks
        self.calls = []
        self.open = False
        self.loop = None
        self.clock = Clock()
        self.connection_id = "synthetic-connection"
        self.instances.append(self)

    async def start(self):
        self.loop = asyncio.get_running_loop()
        self.callbacks["on_connected"](self.connection_id)

    async def stop(self):
        pass

    async def send_audio(self, pcm, end=False):
        self.calls.append(("end" if end else "audio", bytes(pcm)))
        return True

    async def interrupt(self):
        self.calls.append(("interrupt", b""))
        return True

    def note_request_state(self, opened):
        self.open = opened


class LifecycleTester(AsyncExtensionTester):
    def __init__(self, case):
        super().__init__()
        self.case = case
        self.acks = asyncio.Queue()
        self.loop = None
        self.failure = None
        self.sink = None

    async def on_start(self, ten_env):
        self.env = ten_env
        self.loop = asyncio.get_running_loop()
        try:
            for _ in range(200):
                if RecordingSession.instances:
                    self.sink = RecordingSession.instances[-1]
                    if self.sink.loop is not None:
                        break
                await asyncio.sleep(0.005)
            assert self.sink is not None and self.sink.loop is not None
            await self.run_case()
        except Exception as exc:  # Return failures to pytest, not os._exit.
            self.failure = exc
        finally:
            ten_env.stop_test()

    async def on_cmd(self, ten_env, cmd):
        # The production flush hook forwards the command downstream.
        await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

    async def snapshot(self, advance_ms=0):
        async def read_on_extension_loop():
            self.sink.clock.value += advance_ms / 1000
            for _ in range(12):
                await asyncio.sleep(0)
            return list(self.sink.calls), self.sink.open

        future = asyncio.run_coroutine_threadsafe(
            read_on_extension_loop(), self.sink.loop
        )
        return await asyncio.wrap_future(future)

    async def audio(self, pcm, metadata):
        frame = AudioFrame.create("pcm_frame")
        frame.set_sample_rate(RATE)
        frame.set_bytes_per_sample(2)
        frame.set_number_of_channels(1)
        frame.set_samples_per_channel(len(pcm) // 2)
        frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
        frame.set_timestamp(0)
        if metadata is not None:
            frame.set_property_from_json("metadata", json.dumps(metadata))
        frame.alloc_buf(len(pcm))
        buffer = frame.lock_buf()
        buffer[:] = pcm
        frame.unlock_buf(buffer)
        assert await self.env.send_audio_frame(frame) is None
        assert await asyncio.wait_for(self.acks.get(), 3) == (
            "audio",
            metadata,
        )
        return await self.snapshot()

    async def end(self, request, metadata, reason=1, name="tts_audio_end"):
        data = Data.create(name)
        payload = {"request_id": request, "reason": reason}
        if metadata is not None:
            payload["metadata"] = metadata
        data.set_property_from_json(None, json.dumps(payload))
        assert await self.env.send_data(data) is None
        assert await asyncio.wait_for(self.acks.get(), 3) == ("data", None)
        return await self.snapshot()

    async def flush(self):
        result, error = await self.env.send_cmd(Cmd.create("flush"))
        assert error is None
        assert result is not None
        assert result.get_status_code() == StatusCode.OK
        return await self.snapshot()

    async def run_case(self):
        if self.case in ("legacy", "turn_zero", "finalize"):
            metadata = None if self.case == "legacy" else META_A
            if self.case == "turn_zero":
                metadata = {**META_A, "turn_id": 0}
            await self.audio(PCM_A, metadata)
            name = "finalize" if self.case == "finalize" else "tts_audio_end"
            calls, opened = await self.end("request-A", metadata, name=name)
            assert calls == [("audio", PCM_A), ("end", b"")]
            assert not opened
            return

        if self.case == "flush_before_old_first_pcm":
            await self.flush()
            await self.audio(PCM_A, META_B)
            await self.audio(PCM_A * 2, META_A)
            await self.audio(PCM_B * 3, META_B)
            calls, opened = await self.end("request-B", META_B)
            assert calls == [
                ("interrupt", b""),
                ("audio", PCM_A),
                ("audio", PCM_B * 3),
                ("end", b""),
            ]
            assert not opened
            return

        await self.audio(PCM_A, META_A)
        if self.case == "flush_late_pcm":
            await self.flush()
            calls, opened = await self.audio(PCM_A, META_A)
            assert calls == [("audio", PCM_A), ("interrupt", b"")]
            assert not opened
            await self.end("request-A", META_A)
            await self.audio(PCM_B, META_B)
            calls, opened = await self.end("request-B", META_B)
            assert calls[-2:] == [("audio", PCM_B), ("end", b"")]
            assert not opened
            return

        if self.case == "current_reason2":
            await self.flush()
            await self.audio(PCM_B, META_B)
            _, opened = await self.end("request-A", META_A, reason=2)
            assert opened
            calls, opened = await self.end("request-B", META_B, reason=2)
            assert calls[-2:] == [("audio", PCM_B), ("end", b"")]
            assert not opened
            return

        await self.end("request-A", META_A)
        if self.case == "flush_gate":
            calls, _ = await self.audio(PCM_B, META_B)
            assert calls == [("audio", PCM_A), ("end", b"")]
            calls, opened = await self.flush()
            assert calls[-1] == ("interrupt", b"")
            assert len(calls) == 3
            assert not opened
            return

        await self.snapshot(advance_ms=5000)
        metadata = {
            "same_turn": META_A,
            "next_turn": META_B,
            "other_session": {**META_A, "session_id": "other-session"},
        }[self.case]
        await self.audio(PCM_B, metadata)
        calls, opened = await self.end("request-A", META_A)
        assert calls == [
            ("audio", PCM_A),
            ("end", b""),
            ("audio", PCM_B),
        ]
        assert opened, "old request end closed the new request"
        calls, opened = await self.end("request-B", metadata)
        assert calls[-1] == ("end", b"")
        assert len(calls) == 4
        assert not opened


@pytest.mark.parametrize(
    "case",
    [
        "legacy",
        "turn_zero",
        "same_turn",
        "next_turn",
        "other_session",
        "flush_late_pcm",
        "current_reason2",
        "flush_gate",
        "flush_before_old_first_pcm",
        "finalize",
    ],
)
def test_request_lifecycle_through_native_callbacks(monkeypatch, case):
    RecordingSession.instances.clear()
    tester = LifecycleTester(case)
    real_pacer = extension.Pacer

    def make_pacer(*args, **kwargs):
        sink = kwargs.get("sink", args[0] if args else None)
        kwargs.update(clock=sink.clock.now, sleep=sink.clock.sleep)
        return real_pacer(*args, **kwargs)

    monkeypatch.setattr(extension, "SessionClient", RecordingSession)
    monkeypatch.setattr(extension, "Pacer", make_pacer)
    cls = extension.SpatialRealAvatarExtension
    original_audio, original_data = cls.on_audio_frame, cls.on_data

    async def observed_audio(self, ten_env, frame):
        await original_audio(self, ten_env, frame)
        raw, error = frame.get_property_to_json("metadata")
        metadata = json.loads(raw) if raw and error is None else None
        tester.loop.call_soon_threadsafe(
            tester.acks.put_nowait, ("audio", metadata)
        )

    async def observed_data(self, ten_env, data):
        await original_data(self, ten_env, data)
        tester.loop.call_soon_threadsafe(tester.acks.put_nowait, ("data", None))

    monkeypatch.setattr(cls, "on_audio_frame", observed_audio)
    monkeypatch.setattr(cls, "on_data", observed_data)
    tester.set_test_mode_single(
        "spatialreal_avatar_python",
        json.dumps(
            {
                "channel": "synthetic-channel",
                "params": {
                    "spatialreal_api_key": "synthetic-not-a-real-key",
                    "spatialreal_avatar_id": "synthetic-avatar",
                    "agora_appid": "synthetic-app",
                    "agora_uid": "1001",
                },
            }
        ),
    )
    error = tester.run()
    assert error is None, error.error_message()
    if tester.failure is not None:
        raise tester.failure
