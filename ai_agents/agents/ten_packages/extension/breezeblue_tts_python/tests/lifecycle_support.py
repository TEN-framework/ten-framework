#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Shared fixtures for adapter-level lifecycle tests.

Everything here drives the real extension; only the HTTP layer is faked.
"""
import asyncio
import json
import shutil
import tempfile
import threading
from datetime import datetime, timedelta
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

from ten_runtime import ExtensionTester, TenEnvTester, Data
from ten_ai_base.struct import TTSFlush, TTSTextInput

SAMPLE_RATE = 24000
BYTES_PER_SAMPLE = 2
CHANNELS = 1

REASON_REQUEST_END = 1
REASON_INTERRUPTED = 2
REASON_ERROR = 3


def expected_duration_ms(num_bytes: int) -> int:
    return int(num_bytes * 1000 / (SAMPLE_RATE * BYTES_PER_SAMPLE * CHANNELS))


def make_config(dump_dir: str | None = None) -> dict:
    config: dict = {
        "params": {
            "api_key": "brz_test",
            "voice_id": "voc_test",
            "language_code": "en",
        }
    }
    if dump_dir:
        config["dump"] = True
        config["dump_path"] = dump_dir
    return config


class FakeClock:
    """Controlled replacement for ten_ai_base.tts2_http.datetime."""

    def __init__(self):
        self.now_value = datetime(2026, 1, 1)

    def advance(self, milliseconds: int) -> None:
        self.now_value += timedelta(milliseconds=milliseconds)

    def patch(self):
        clock = self

        class FakeDatetime(datetime):
            @classmethod
            def now(cls, tz=None):
                return clock.now_value

        return patch("ten_ai_base.tts2_http.datetime", FakeDatetime)


class BlockedStream:
    """A request stuck waiting for response headers."""

    def __init__(self):
        self.interrupted = False

    async def __aenter__(self):
        try:
            await asyncio.sleep(3600)
        except asyncio.CancelledError:
            self.interrupted = True
            raise

    async def __aexit__(self, *exc):
        return False


def make_response(
    chunks=(),
    status=200,
    body=b"",
    headers=None,
    fail_after=None,
    on_chunk=None,
    delay=0.0,
    before_fail=None,
    on_enter=None,
    on_read=None,
):
    """A fake streaming httpx response.

    on_enter / on_read / on_chunk / before_fail are hooks used to advance a
    FakeClock at precise points of the request.
    """
    response = AsyncMock()
    response.status_code = status
    response.headers = headers or {"content-type": "audio/pcm"}

    async def aread():
        if on_read:
            on_read()
        return body

    response.aread = aread
    response.aclose = AsyncMock()

    async def aiter_bytes():
        for index, chunk in enumerate(chunks):
            if on_chunk:
                on_chunk(index)
            yield chunk
            await asyncio.sleep(delay)
        if fail_after is not None:
            if before_fail:
                before_fail()
            raise fail_after

    def enter(*args):
        if on_enter:
            on_enter()
        return response

    response.aiter_bytes = aiter_bytes
    response.__aenter__ = AsyncMock(side_effect=enter)
    response.__aexit__ = AsyncMock(return_value=None)
    return response


class Scripted(ExtensionTester):
    """Sends scripted inputs and records every event the extension emits.

    inputs:   list of (request_id, text, text_input_end).
    mode:     "all"        send every input immediately;
              "sequential" send the next input after each tts_audio_end.
    flush_at: optional ("first_audio" | seconds, request_id): send tts_flush.
    after_flush: inputs sent once tts_flush_end arrives (recovery check).
    stop_after_ends: stop once this many tts_audio_end events were seen.
    """

    def __init__(
        self,
        inputs,
        stop_after_ends,
        mode="all",
        flush_at=None,
        after_flush=(),
        on_first_audio=None,
        timeout_s=15.0,
    ):
        super().__init__()
        self.inputs = list(inputs)
        self.stop_after_ends = stop_after_ends
        self.mode = mode
        self.flush_at = flush_at
        self.after_flush = list(after_flush)
        self.on_first_audio = on_first_audio
        self.timeout_s = timeout_s

        self.order: list[str] = []
        self.starts: list[dict] = []
        self.ends: list[dict] = []
        self.errors: list[dict] = []
        self.flush_started = False
        self.flush_ended = False
        self.audio: dict[str, bytearray] = {}
        self.audio_after_flush_end = False
        self.frame_sizes: list[int] = []
        self.timed_out = False
        self._finished = False
        self._current: str | None = None
        self._next = 0
        self._flush_sent = False
        self._first_audio_seen = False

    # -- sending ---------------------------------------------------------
    def _send(self, ten_env, request_id, text, end) -> None:
        msg = TTSTextInput(request_id=request_id, text=text, text_input_end=end)
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, msg.model_dump_json())
        ten_env.send_data(data)

    def _send_flush(self, ten_env, request_id) -> None:
        if self._flush_sent:
            return
        self._flush_sent = True
        data = Data.create("tts_flush")
        data.set_property_from_json(
            None, TTSFlush(flush_id=request_id).model_dump_json()
        )
        ten_env.send_data(data)

    def _send_next(self, ten_env) -> None:
        if self._next < len(self.inputs):
            self._send(ten_env, *self.inputs[self._next])
            self._next += 1

    # -- ExtensionTester hooks --------------------------------------------
    def on_start(self, ten_env: TenEnvTester) -> None:
        if self.mode == "all":
            for item in self.inputs:
                self._send(ten_env, *item)
            self._next = len(self.inputs)
        else:
            self._send_next(ten_env)

        if self.flush_at and isinstance(self.flush_at[0], (int, float)):
            seconds, rid = self.flush_at
            self._timer(seconds, lambda: self._send_flush(ten_env, rid))
        # Never hang the suite: fail by assertion instead.
        self._timer(self.timeout_s, lambda: self._timeout(ten_env))
        ten_env.on_start_done()

    @staticmethod
    def _timer(seconds, fn) -> None:
        # Daemon: a leftover timer must not keep the process alive.
        timer = threading.Timer(seconds, fn)
        timer.daemon = True
        timer.start()

    def _timeout(self, ten_env) -> None:
        if self._finished:
            return
        self.timed_out = True
        self._stop(ten_env)

    def _stop(self, ten_env) -> None:
        self._finished = True
        try:
            ten_env.stop_test()
        except Exception:  # already stopped (late timer)
            pass

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame) -> None:
        # Only audio of the flushed request counts: a request sent after the
        # flush legitimately produces audio.
        if (
            self.flush_ended
            and self.flush_at
            and self._current == self.flush_at[1]
        ):
            self.audio_after_flush_end = True
        buf = audio_frame.lock_buf()
        try:
            self.frame_sizes.append(len(buf))
            self.audio.setdefault(self._current or "?", bytearray()).extend(
                bytes(buf)
            )
        finally:
            audio_frame.unlock_buf(buf)

        if not self._first_audio_seen:
            self._first_audio_seen = True
            if self.on_first_audio:
                self.on_first_audio()
            if self.flush_at and self.flush_at[0] == "first_audio":
                self._send_flush(ten_env, self.flush_at[1])

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        raw, _ = data.get_property_to_json("")
        payload = json.loads(raw) if raw else {}

        if name == "tts_audio_start":
            self._current = payload.get("request_id")
            self.order.append(f"start:{self._current}")
            self.starts.append(payload)
        elif name == "tts_audio_end":
            self.order.append(f"end:{payload.get('request_id')}")
            self.ends.append(payload)
            if len(self.ends) >= self.stop_after_ends:
                self._stop(ten_env)
            elif self.mode == "sequential":
                self._send_next(ten_env)
        elif name == "error":
            self.order.append("error")
            self.errors.append(payload)
        elif name == "tts_flush_start":
            self.order.append("flush_start")
            self.flush_started = True
        elif name == "tts_flush_end":
            self.order.append("flush_end")
            self.flush_ended = True
            for item in self.after_flush:
                self._send(ten_env, *item)


def run_scripted(inputs, responses, stop_after_ends, dump=False, **kwargs):
    """Run the real extension against a fake HTTP client.

    `responses` is a list consumed in order (or a callable(*args, **kwargs)).
    Returns (tester, http_client, dump_dir).
    """
    dump_dir = tempfile.mkdtemp(prefix="breeze_dump_") if dump else None
    try:
        with patch(
            "breezeblue_tts_python.breezeblue_tts.AsyncClient"
        ) as MockAsyncClient:
            http = AsyncMock()
            if callable(responses):
                http.stream = MagicMock(side_effect=responses)
            else:
                http.stream = MagicMock(side_effect=list(responses))
            http.aclose = AsyncMock()
            MockAsyncClient.return_value = http

            tester = Scripted(inputs, stop_after_ends, **kwargs)
            tester.set_test_mode_single(
                "breezeblue_tts_python", json.dumps(make_config(dump_dir))
            )
            tester.run()
        dumps = {}
        if dump_dir:
            for path in Path(dump_dir).glob("breezeblue_dump_*.pcm"):
                dumps[path.stem.removeprefix("breezeblue_dump_")] = (
                    path.read_bytes()
                )
        return tester, http, dumps
    finally:
        if dump_dir:
            shutil.rmtree(dump_dir, ignore_errors=True)
