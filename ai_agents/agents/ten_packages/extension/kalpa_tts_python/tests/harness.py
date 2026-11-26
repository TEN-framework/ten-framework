#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Runs the extension in the TEN test runtime and records what it emits."""
import asyncio
import json
from typing import Any, Awaitable, Callable

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)

REQUEST_END, INTERRUPTED, ERROR = 1, 2, 3
FATAL, NON_FATAL = -1000, 1000
OK = REQUEST_END


class Recording(AsyncExtensionTester):
    """Runs `script`, waits for `done` if given, then stops the extension."""

    def __init__(self, script, done, timeout_s: float) -> None:
        super().__init__()
        self._script = script
        self._done = done
        self._timeout_s = timeout_s
        self._tasks: list[asyncio.Task] = []
        self._env: AsyncTenEnvTester | None = None
        # Everything received, in order: `(name, payload)`. An audio frame
        # appears as `("pcm_frame", {...})` with its format.
        self.events: list[tuple[str, dict[str, Any]]] = []
        self.audio = bytearray()

    def names(self) -> list[str]:
        return [name for name, _ in self.events]

    def of(self, name: str) -> list[dict[str, Any]]:
        return [payload for event, payload in self.events if event == name]

    def before(self, first: str, second: str) -> bool:
        return self.names().index(first) < self.names().index(second)

    def ends(self) -> list[tuple[str, int]]:
        """`(request id, reason)` of each `tts_audio_end`."""
        return [
            (e["request_id"], e["reason"]) for e in self.of("tts_audio_end")
        ]

    def ended(self, request_id: str) -> bool:
        return any(end[0] == request_id for end in self.ends())

    def errors(self) -> list[tuple[str, int, str]]:
        """`(request id, code, Kalpa's error type)` of each error."""
        return [
            (e["id"], e["code"], e["vendor_info"]["code"])
            for e in self.of("error")
        ]

    def metric(self, name: str) -> list[tuple[int, int]]:
        """`(turn_id, value)` of each report of one metric for a request."""
        return [
            (m["metadata"]["turn_id"], m["metrics"][name])
            for m in self.of("metrics")
            if name in m.get("metrics", {}) and "turn_id" in m["metadata"]
        ]

    async def say(self, request_id: str, text: str, end: bool = True) -> None:
        """Send text for `r<turn>`; `end` marks the last of the request."""
        turn = int("".join(filter(str.isdigit, request_id)))
        await self._send(
            "tts_text_input",
            request_id=request_id,
            text=text,
            text_input_end=end,
            metadata={"session_id": "session", "turn_id": turn},
        )

    async def flush(self, wait: bool = True) -> None:
        """Send a flush and wait until the extension reports it done."""
        flushed = len(self.of("tts_flush_end"))
        await self._send(
            "tts_flush", flush_id="flush", metadata={"session_id": "session"}
        )
        if wait:
            await self.until(lambda: len(self.of("tts_flush_end")) > flushed)

    async def _send(self, name: str, **payload: Any) -> None:
        data = Data.create(name)
        data.set_property_from_json("", json.dumps(payload))
        await self._env.send_data(data)

    async def until(self, condition: Callable[[], Any]) -> None:
        deadline = asyncio.get_running_loop().time() + 10.0
        while not condition():
            if asyncio.get_running_loop().time() > deadline:
                raise TimeoutError(f"condition not met; got {self.names()}")
            await asyncio.sleep(0.01)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        self._env = ten_env

        def fail(message: str) -> None:
            ten_env.stop_test(
                TenError.create(TenErrorCode.ErrorCodeGeneric, message)
            )

        async def run_script() -> None:
            try:
                await self._script(self)
                if self._done is not None:
                    await self.until(lambda: self._done(self))
            except Exception as exc:
                fail(f"script failed: {type(exc).__name__}: {exc}")
                return
            ten_env.stop_test()

        async def watchdog() -> None:
            await asyncio.sleep(self._timeout_s)
            fail(f"timed out; received {self.names()}")

        self._tasks = [
            asyncio.create_task(run_script()),
            asyncio.create_task(watchdog()),
        ]

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        for task in self._tasks:
            task.cancel()

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        payload, _ = data.get_property_to_json("")
        self.events.append((data.get_name(), json.loads(payload or "{}")))

    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        buffer = audio_frame.lock_buf()
        try:
            self.audio += bytes(buffer)
        finally:
            audio_frame.unlock_buf(buffer)
        frame = {
            "sample_rate": audio_frame.get_sample_rate(),
            "channels": audio_frame.get_number_of_channels(),
            "bytes_per_sample": audio_frame.get_bytes_per_sample(),
        }
        self.events.append(("pcm_frame", frame))


def run(
    config: dict[str, Any],
    script: Callable[[Recording], Awaitable[None]],
    done: Callable[[Recording], Any] | str | None = None,
) -> Recording:
    """Run the extension with `config` and return what it emitted.

    `done` is a condition to wait for after the script, or the id of a
    request whose `tts_audio_end` to wait for.
    """
    if isinstance(done, str):
        request_id = done
        done = lambda rec: rec.ended(request_id)
    recording = Recording(script, done, 15.0)
    recording.set_test_mode_single("kalpa_tts_python", json.dumps(config))
    error = recording.run()
    assert error is None, error.error_message()
    return recording
