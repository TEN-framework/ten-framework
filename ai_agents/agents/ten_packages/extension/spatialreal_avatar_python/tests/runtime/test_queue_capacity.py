#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Native callbacks preserve a startup burst or explicitly cancel overflow.

The shared FakeApp fixture and addon registration come from the runtime
suite. Production hooks, sender, resampler and pacer remain unchanged;
only the external session, clock and test queue limits are substituted.
"""

import asyncio
from array import array
import hashlib
import json
import sys

import pytest
from ten_runtime import AudioFrame, AudioFrameDataFmt, Data

from test_request_lifecycle import (
    LifecycleTester,
    META_A,
    META_B,
    PCM_A,
    RecordingSession,
    extension,
)


class BlockedSession(RecordingSession):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.release_connection = None
        self.release_interrupt = None
        self.interrupt_started = False
        self.overflow_task = None
        self.cancel_hook_on_stop = False
        self.cancelled_hook_finished = False
        self.stopped = False
        self.instance = None

    async def start(self):
        self.loop = asyncio.get_running_loop()
        self.release_connection = asyncio.Event()
        self.release_interrupt = asyncio.Event()
        await self.release_connection.wait()
        self.callbacks["on_connected"](self.connection_id)

    async def interrupt(self):
        self.calls.append(("interrupt", b""))
        self.interrupt_started = True
        await self.release_interrupt.wait()
        return True

    async def stop(self):
        self.stopped = True
        if self.cancel_hook_on_stop and self.overflow_task is not None:
            # Simulate shutdown cancelling an in-flight external operation.
            # A CancelledError escaping the production hook makes the real
            # TEN wrapper exit the process, so pytest cannot falsely pass.
            assert self.instance._stopping
            assert not self.overflow_task.done()
            self.overflow_task.cancel()
            await self.overflow_task
            self.cancelled_hook_finished = True


class CapacityTester(LifecycleTester):
    def __init__(self, case):
        super().__init__(case)
        self.module_errors = []
        self.report = {}

    async def on_data(self, _ten_env, data):
        if data.get_name() == "error":
            raw, error = data.get_property_to_json(None)
            assert error is None
            self.module_errors.append(json.loads(raw))

    async def control(self, callback):
        async def execute():
            result = callback()
            for _ in range(15):
                await asyncio.sleep(0)
            return result

        future = asyncio.run_coroutine_threadsafe(execute(), self.sink.loop)
        return await asyncio.wrap_future(future)

    async def wait_until(self, predicate):
        for _ in range(500):
            if await self.control(predicate):
                return
            await asyncio.sleep(0.005)
        raise AssertionError("native queue test condition did not settle")

    async def enqueue_audio(self, pcm, metadata, rate=24000):
        frame = AudioFrame.create("pcm_frame")
        frame.set_sample_rate(rate)
        frame.set_bytes_per_sample(2)
        frame.set_number_of_channels(1)
        frame.set_samples_per_channel(len(pcm) // 2)
        frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
        frame.set_timestamp(0)
        frame.set_property_from_json("metadata", json.dumps(metadata))
        frame.alloc_buf(len(pcm))
        buffer = frame.lock_buf()
        buffer[:] = pcm
        frame.unlock_buf(buffer)
        assert await self.env.send_audio_frame(frame) is None

    async def enqueue_end(self, request, metadata):
        data = Data.create("tts_audio_end")
        data.set_property_from_json(
            None,
            json.dumps(
                {"request_id": request, "reason": 1, "metadata": metadata}
            ),
        )
        assert await self.env.send_data(data) is None

    async def hook_completions(self, count):
        for _ in range(count):
            await asyncio.wait_for(self.acks.get(), 5)

    async def run_case(self):
        if self.case == "startup_burst":
            samples = array(
                "h", ((i * 137) % 60001 - 30000 for i in range(44100 * 26))
            )
            if sys.byteorder != "little":
                samples.byteswap()
            pcm = samples.tobytes()
            chunks = [pcm[i : i + 8820] for i in range(0, len(pcm), 8820)]
            # No hook ACK, snapshot or scheduling barrier between frames or
            # End: exercise the real independent TEN callback dispatch.
            for chunk in chunks:
                await self.enqueue_audio(chunk, META_A, rate=44100)
            await self.enqueue_end("startup-A", META_A)
            await self.hook_completions(len(chunks) + 1)
            assert not self.sink.calls
            assert not self.module_errors
            await self.snapshot(advance_ms=41000)
            assert not self.sink.calls, "virtual time must not imply readiness"
            await self.control(self.sink.release_connection.set)
            await self.wait_until(
                lambda: bool(self.sink.calls)
                and self.sink.calls[-1][0] == "end"
            )
            calls, opened = await self.snapshot()
            from ten_packages.extension.spatialreal_avatar_python.audio import (
                Resampler,
            )

            expected = Resampler(44100, 24000).convert(pcm)
            actual = b"".join(data for kind, data in calls if kind == "audio")
            assert actual == expected
            assert calls[-1] == ("end", b"")
            assert sum(kind == "end" for kind, _ in calls) == 1
            assert not opened
            assert not self.module_errors
            self.report = {
                "input_samples_44100": len(pcm) // 2,
                "input_chunks": len(chunks),
                "virtual_wait_ms": 41000,
                "output_samples_24000": len(actual) // 2,
                "sha256": hashlib.sha256(actual).hexdigest(),
                "end_last": True,
            }
            return

        # The held first frame counts toward a deliberately small budget.
        await self.enqueue_audio(PCM_A, META_A)
        await self.enqueue_end("old-A", META_A)
        await self.hook_completions(2)
        await self.enqueue_audio(PCM_A, META_B)
        await self.wait_until(lambda: self.sink.interrupt_started)
        assert self.sink.calls == [("interrupt", b"")]

        if self.case == "overflow_stop_cancel":
            self.sink.cancel_hook_on_stop = True
            # The inherited tester calls stop_test after this returns.
            # Production on_stop -> fake stop cancels the waiting hook.
            return

        # A later end can change the extension's current turn while the
        # overflow hook is awaiting interrupt. Error identity must stay B.
        meta_c = {**META_A, "turn_id": 3}
        await self.enqueue_end("concurrent-C", meta_c)
        await self.hook_completions(1)
        await self.control(self.sink.release_interrupt.set)
        await self.hook_completions(1)
        await self.wait_until(lambda: len(self.module_errors) == 1)
        error = self.module_errors[0]
        assert error["code"] == extension.CODE_NON_FATAL
        assert error["vendor_info"]["code"] == "InputQueueOverflow"
        assert error["id"] == str(META_B["turn_id"])
        assert error["metadata"]["session_id"] == META_B["session_id"]
        assert not any(kind == "end" for kind, _ in self.sink.calls)
        await self.control(self.sink.release_connection.set)
        await self.wait_until(
            lambda: self.sink.instance.sender._inflight_item is None
        )
        fresh = {**META_A, "turn_id": 4}
        await self.enqueue_audio(PCM_A, fresh)
        await self.enqueue_end("fresh-D", fresh)
        await self.hook_completions(2)
        await self.wait_until(lambda: self.sink.calls[-1][0] == "end")
        assert self.sink.calls == [
            ("interrupt", b""),
            ("audio", PCM_A),
            ("end", b""),
        ]
        self.report = {"overflow_turn": 2, "fresh_turn_recovered": True}


@pytest.mark.parametrize(
    "case", ["startup_burst", "overflow_recovery", "overflow_stop_cancel"]
)
def test_queue_capacity_through_native_callbacks(monkeypatch, request, case):
    RecordingSession.instances.clear()
    tester = CapacityTester(case)
    real_pacer = extension.Pacer
    real_sender = extension.AudioSender

    def make_pacer(*args, **kwargs):
        sink = kwargs.get("sink", args[0] if args else None)
        kwargs.update(clock=sink.clock.now, sleep=sink.clock.sleep)
        return real_pacer(*args, **kwargs)

    def make_sender(*args, **kwargs):
        if case != "startup_burst":
            kwargs["max_queue_ms"] = 150
        sender = real_sender(*args, **kwargs)
        return sender

    monkeypatch.setattr(extension, "SessionClient", BlockedSession)
    monkeypatch.setattr(extension, "Pacer", make_pacer)
    monkeypatch.setattr(extension, "AudioSender", make_sender)
    cls = extension.SpatialRealAvatarExtension
    original_audio = cls.on_audio_frame
    original_data = cls.on_data
    original_start = cls.on_start

    async def observed_start(self, ten_env):
        await original_start(self, ten_env)
        if self.client is not None:
            self.client.instance = self

    async def observed_audio(self, ten_env, frame):
        raw, error = frame.get_property_to_json("metadata")
        metadata = json.loads(raw) if raw and error is None else None
        if metadata == META_B:
            self.client.overflow_task = asyncio.current_task()
        await original_audio(self, ten_env, frame)
        tester.loop.call_soon_threadsafe(
            tester.acks.put_nowait, ("audio", metadata)
        )

    async def observed_data(self, ten_env, data):
        await original_data(self, ten_env, data)
        tester.loop.call_soon_threadsafe(tester.acks.put_nowait, ("data", None))

    monkeypatch.setattr(cls, "on_start", observed_start)
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
    assert tester.sink.stopped
    if case == "overflow_stop_cancel":
        assert tester.sink.cancelled_hook_finished
        assert not tester.module_errors
        assert tester.sink.calls == [("interrupt", b"")]
        tester.report = {"cancelled_hook_finished": True, "normal_ends": 0}
    for key, value in tester.report.items():
        request.node.user_properties.append((key, value))
