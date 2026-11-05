#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Terminal accounting at the extension boundary with a controlled clock.

The scripted vendor stream advances a fake clock, so start-to-end
intervals and audio durations are exact rather than "at least" checks.
"""

import asyncio
import json
from datetime import datetime, timedelta
from unittest.mock import patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenEnvTester,
)

from blaze_tts_python.blaze_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)
from blaze_tts_python.tests.mock import (
    AUDIO_CHUNK,
    BASIC_CONFIG,
    RecordingTester,
    install_mock_client,
    send_text,
)

REQUEST_END, INTERRUPTED, ERROR = 1, 2, 3
CHUNK_MS = 20  # AUDIO_CHUNK is 20 ms of 24 kHz PCM16 mono.
BASE_TIME = datetime(2026, 1, 1)


class Clock:
    def __init__(self):
        self.ms = 0

    def advance(self, ms: int) -> None:
        self.ms += ms

    def datetime_class(self):
        clock = self

        class FakeDatetime(datetime):
            @classmethod
            def now(cls, tz=None):
                del tz
                return BASE_TIME + timedelta(milliseconds=clock.ms)

        return FakeDatetime


def _run_with_stream(stream, inputs, expected_ends=1, tester=None):
    clock = Clock()
    with patch(
        "blaze_tts_python.extension.BlazeTTSClient"
    ) as client_cls, patch(
        "blaze_tts_python.extension.datetime", clock.datetime_class()
    ):
        instance = install_mock_client(client_cls)

        async def scripted_get(text: str):
            async for item in stream(text, clock):
                yield item

        instance.get.side_effect = scripted_get
        tester = tester or RecordingTester(inputs, expected_ends=expected_ends)
        tester.set_test_mode_single(
            "blaze_tts_python", json.dumps(BASIC_CONFIG)
        )
        tester.run()
    return tester


def _end(tester, request_id):
    ends = [
        p
        for p in tester.payloads("tts_audio_end")
        if p["request_id"] == request_id
    ]
    assert len(ends) == 1, ends
    return ends[0]


def test_success_interval_runs_from_first_audio_to_end():
    async def stream(_text, clock):
        clock.advance(100)
        yield 100, EVENT_TTS_TTFB_METRIC
        for _ in range(3):
            yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
        clock.advance(300)
        yield None, EVENT_TTS_END

    tester = _run_with_stream(stream, [("req-1", "Xin chào.", True)])

    end = _end(tester, "req-1")
    assert end["reason"] == REQUEST_END
    assert end["request_event_interval_ms"] == 300
    assert end["request_total_audio_duration_ms"] == 3 * CHUNK_MS


def test_error_after_partial_audio_keeps_exact_accounting():
    async def stream(_text, clock):
        clock.advance(100)
        yield 100, EVENT_TTS_TTFB_METRIC
        yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
        yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
        clock.advance(50)
        yield b"internal-error", EVENT_TTS_ERROR

    tester = _run_with_stream(stream, [("req-1", "Xin chào.", True)])

    end = _end(tester, "req-1")
    assert end["reason"] == ERROR
    assert end["request_event_interval_ms"] == 50
    assert end["request_total_audio_duration_ms"] == 2 * CHUNK_MS
    assert tester.count("error") == 1


def test_no_audio_error_has_adjacent_start_and_end():
    async def stream(_text, clock):
        # The vendor hangs, then the client times out with no audio.
        clock.advance(30_000)
        yield b"Timeout waiting for Blaze TTS audio", EVENT_TTS_ERROR

    tester = _run_with_stream(stream, [("req-1", "Xin chào.", True)])

    end = _end(tester, "req-1")
    assert end["reason"] == ERROR
    assert end["request_event_interval_ms"] == 0
    assert end["request_total_audio_duration_ms"] == 0
    assert tester.count("tts_audio_start") == 1


class FlushTester(RecordingTester):
    """Flush req-1 after its first audio, then send req-2."""

    def __init__(self):
        super().__init__([("req-1", "Một câu rất dài.", True)])
        self.timeline: list[tuple[str, str | None]] = []
        self.req1_audio_bytes = 0
        self.req1_ended = False

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}
        self.events.append((name, payload))
        self.timeline.append((name, payload.get("request_id")))
        if name == "tts_audio_start" and payload["request_id"] == "req-1":
            flush = Data.create("tts_flush")
            flush.set_property_from_json(
                None, json.dumps({"flush_id": "flush-1", "metadata": {}})
            )
            ten_env.send_data(flush)
        if name == "tts_audio_end" and payload["request_id"] == "req-1":
            self.req1_ended = True
            send_text(ten_env, "req-2", "Câu sau.", True)
        if name == "tts_audio_end" and payload["request_id"] == "req-2":
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        self.timeline.append(("audio", None))
        if not self.req1_ended:
            buf = audio_frame.lock_buf()
            try:
                self.req1_audio_bytes += len(bytes(buf))
            finally:
                audio_frame.unlock_buf(buf)
        super().on_audio_frame(ten_env, audio_frame)


def test_flush_interrupts_once_drops_stale_audio_and_next_request_works():
    async def stream(text, clock):
        clock.advance(100)
        yield 100, EVENT_TTS_TTFB_METRIC
        if text.startswith("Một"):
            # A long stream that is still going when the flush arrives.
            for _ in range(200):
                # Each chunk is 20 ms of audio arriving 20 ms apart.
                clock.advance(CHUNK_MS)
                yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
                await asyncio.sleep(0.01)
        else:
            yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
        clock.advance(40)
        yield None, EVENT_TTS_END

    tester = _run_with_stream(stream, [], tester=FlushTester())

    interrupted = _end(tester, "req-1")
    assert interrupted["reason"] == INTERRUPTED
    assert _end(tester, "req-2")["reason"] == REQUEST_END
    assert tester.count("tts_flush_end") == 1

    # Accounting matches the PCM actually emitted before the interrupt.
    emitted_ms = tester.req1_audio_bytes // (24000 * 2 // 1000)
    assert emitted_ms > 0
    assert interrupted["request_total_audio_duration_ms"] == emitted_ms
    assert interrupted["request_event_interval_ms"] == emitted_ms

    # No req-1 audio may follow its end: everything between the req-1
    # end and the req-2 start must be free of audio frames.
    names = tester.timeline
    req1_end = names.index(("tts_audio_end", "req-1"))
    req2_start = names.index(("tts_audio_start", "req-2"))
    assert ("audio", None) not in names[req1_end:req2_start]


def _run_sequential_with_dump(dump_path, flush_error=None):
    config = {**BASIC_CONFIG, "dump": True, "dump_path": dump_path}
    with patch("blaze_tts_python.extension.BlazeTTSClient") as client_cls:
        install_mock_client(client_cls)
        flush_patch = (
            patch(
                "blaze_tts_python.extension.PCMWriter.flush",
                side_effect=flush_error,
            )
            if flush_error is not None
            else None
        )
        if flush_patch:
            flush_patch.start()
        try:
            tester = RecordingTester(
                [("req-1", "Một.", True), ("req-2", "Hai.", True)],
                expected_ends=2,
            )
            tester.set_test_mode_single("blaze_tts_python", json.dumps(config))
            tester.run()
        finally:
            if flush_patch:
                flush_patch.stop()
    return tester


def test_dump_directory_is_created_and_next_request_still_runs(tmp_path):
    dump_path = tmp_path / "not" / "yet" / "created"
    tester = _run_sequential_with_dump(str(dump_path))

    assert _end(tester, "req-1")["reason"] == REQUEST_END
    assert _end(tester, "req-2")["reason"] == REQUEST_END
    assert (dump_path / "blaze_dump_req-1.pcm").stat().st_size > 0


def test_dump_flush_failure_does_not_block_the_next_request(tmp_path):
    tester = _run_sequential_with_dump(
        str(tmp_path), flush_error=OSError("disk full")
    )

    assert _end(tester, "req-1")["reason"] == REQUEST_END
    assert _end(tester, "req-2")["reason"] == REQUEST_END


class CancelWhileWaitingTester(AsyncExtensionTester):
    """Against the real client: flush req-1 while it waits for its first
    byte, flush req-2 while the client is reconnecting, then req-3 runs."""

    def __init__(self):
        super().__init__()
        self.events: list[tuple[str, dict]] = []
        self.timeline: list[tuple[str, str | None]] = []
        self.flushes = 0
        self.tasks: list[asyncio.Task] = []

    async def _send_text(self, ten_env, request_id, text):
        data = Data.create("tts_text_input")
        data.set_property_from_json(
            None,
            TTSTextInput(
                request_id=request_id, text=text, text_input_end=True
            ).model_dump_json(),
        )
        await ten_env.send_data(data)

    async def _flush_after(self, ten_env, seconds: float) -> None:
        await asyncio.sleep(seconds)
        self.flushes += 1
        data = Data.create("tts_flush")
        data.set_property_from_json(
            None,
            json.dumps({"flush_id": f"flush-{self.flushes}", "metadata": {}}),
        )
        await ten_env.send_data(data)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await self._send_text(ten_env, "req-1", "chậm")
        self.tasks.append(asyncio.create_task(self._flush_after(ten_env, 0.3)))

    async def on_data(self, ten_env: AsyncTenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}
        self.events.append((name, payload))
        self.timeline.append((name, payload.get("request_id")))
        if name != "tts_audio_end":
            return
        if payload["request_id"] == "req-1":
            await self._send_text(ten_env, "req-2", "Hai.")
            self.tasks.append(
                asyncio.create_task(self._flush_after(ten_env, 0.3))
            )
        elif payload["request_id"] == "req-2":
            await self._send_text(ten_env, "req-3", "Ba.")
        elif payload["request_id"] == "req-3":
            ten_env.stop_test()

    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame):
        self.timeline.append(("audio", None))

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        for task in self.tasks:
            task.cancel()

    def count(self, name: str) -> int:
        return sum(1 for event, _ in self.events if event == name)

    def payloads(self, name: str) -> list[dict]:
        return [payload for event, payload in self.events if event == name]


def test_real_client_cancel_while_waiting_and_connecting_then_recovers():
    from blaze_tts_python.tests.mock import ThreadedMockBlazeTTSServer

    async def respond(query, ws, server):
        if query["query"] == "chậm":
            await asyncio.sleep(5)  # no first byte before the flush
        await server.stream(ws)

    # Connection 1 is the preheat; req-1 runs on it and is flushed, which
    # drops the socket. Connection 2 (req-2) is held in its handshake.
    with ThreadedMockBlazeTTSServer(
        valid_token="test_api_key",
        respond=respond,
        greeting_delays={2: 2.0},
    ) as server:
        config = {
            "params": {**BASIC_CONFIG["params"], "base_url": server.base_url}
        }
        tester = CancelWhileWaitingTester()
        tester.set_test_mode_single("blaze_tts_python", json.dumps(config))
        tester.run()

    for request_id in ("req-1", "req-2"):
        end = _end(tester, request_id)
        assert end["reason"] == INTERRUPTED
        assert end["request_total_audio_duration_ms"] == 0
        assert end["request_event_interval_ms"] < 50
    assert _end(tester, "req-3")["reason"] == REQUEST_END
    assert _end(tester, "req-3")["request_total_audio_duration_ms"] > 0
    assert tester.count("tts_flush_end") == 2
    assert tester.count("error") == 0
    # All audio belongs to req-3.
    names = tester.timeline
    assert ("audio", None) not in names[
        : names.index(("tts_audio_start", "req-3"))
    ]
