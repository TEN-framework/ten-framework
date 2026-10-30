import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Adapter-level tests: drive the real extension with a fake HTTP stream and
assert the TEN events it emits (audio start/end accounting, errors, recovery).
"""
import asyncio
import json
from datetime import datetime, timedelta
from unittest.mock import patch, AsyncMock, MagicMock

import httpx

from ten_runtime import ExtensionTester, TenEnvTester, Data
from ten_ai_base.struct import TTSTextInput

SAMPLE_RATE = 24000
BYTES_PER_SAMPLE = 2
CHANNELS = 1

REASON_REQUEST_END = 1
REASON_ERROR = 3
FATAL = -1000
NON_FATAL = 1000

CONFIG = {
    "params": {
        "api_key": "brz_test",
        "voice_id": "voc_test",
        "language_code": "en",
    }
}


def expected_duration_ms(num_bytes: int) -> int:
    return int(num_bytes * 1000 / (SAMPLE_RATE * BYTES_PER_SAMPLE * CHANNELS))


class Recorder(ExtensionTester):
    """Sends a list of requests, one after another, and records every event.

    Each request is sent once the previous one produced its `tts_audio_end`, so
    a request that follows an error/flush proves the extension recovered.
    """

    def __init__(self, texts, stop_after_ends=None):
        super().__init__()
        self.texts = texts
        self.stop_after_ends = stop_after_ends or len(texts)
        self.starts: list[dict] = []
        self.ends: list[dict] = []
        self.errors: list[dict] = []
        self.audio: dict[str, bytearray] = {}
        self.frame_formats: set[tuple[int, int, int]] = set()
        self.order: list[str] = []
        self._next = 0

    def _send_next(self, ten_env) -> None:
        if self._next >= len(self.texts):
            return
        text = self.texts[self._next]
        rid = f"req_{self._next}"
        self._next += 1
        msg = TTSTextInput(request_id=rid, text=text, text_input_end=True)
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, msg.model_dump_json())
        ten_env.send_data(data)

    def on_start(self, ten_env: TenEnvTester) -> None:
        self._send_next(ten_env)
        ten_env.on_start_done()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame) -> None:
        rid = f"req_{self._next - 1}"
        buf = audio_frame.lock_buf()
        try:
            self.audio.setdefault(rid, bytearray()).extend(bytes(buf))
        finally:
            audio_frame.unlock_buf(buf)
        self.frame_formats.add(
            (
                audio_frame.get_sample_rate(),
                audio_frame.get_bytes_per_sample(),
                audio_frame.get_number_of_channels(),
            )
        )

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        raw, _ = data.get_property_to_json("")
        payload = json.loads(raw) if raw else {}
        if name == "tts_audio_start":
            self.order.append("start")
            self.starts.append(payload)
        elif name == "error":
            self.order.append("error")
            self.errors.append(payload)
        elif name == "tts_audio_end":
            self.order.append("end")
            self.ends.append(payload)
            if len(self.ends) >= self.stop_after_ends:
                ten_env.stop_test()
            else:
                self._send_next(ten_env)


def make_response(
    chunks=(),
    status=200,
    body=b"",
    headers=None,
    fail_after=None,
    on_chunk=None,
):
    """A fake streaming httpx response."""
    response = AsyncMock()
    response.status_code = status
    response.headers = headers or {"content-type": "audio/pcm"}
    response.aread = AsyncMock(return_value=body)
    response.aclose = AsyncMock()

    async def aiter_bytes():
        for index, chunk in enumerate(chunks):
            if on_chunk:
                on_chunk(index)
            yield chunk
            await asyncio.sleep(0)
        if fail_after is not None:
            raise fail_after

    response.aiter_bytes = aiter_bytes
    response.__aenter__ = AsyncMock(return_value=response)
    response.__aexit__ = AsyncMock(return_value=None)
    return response


def run(texts, responses, MockAsyncClient, **kwargs):
    client = AsyncMock()
    if callable(responses):
        client.stream = MagicMock(side_effect=responses)
    else:
        client.stream = MagicMock(side_effect=list(responses))
    client.aclose = AsyncMock()
    MockAsyncClient.return_value = client

    tester = Recorder(texts, **kwargs)
    tester.set_test_mode_single("breezeblue_tts_python", json.dumps(CONFIG))
    tester.run()
    return tester, client


# ================ success accounting ================
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_success_duration_counts_emitted_bytes_across_odd_chunks(Mock):
    """Duration is computed once from the bytes actually emitted, even when
    the network splits 16-bit samples across chunk boundaries."""
    # 4800 samples (0.2 s) = 9600 bytes, delivered in odd-sized pieces.
    pcm = bytes(range(256)) * 37 + bytes(128)  # 9600 bytes
    assert len(pcm) == 9600
    pieces = [pcm[:1001], pcm[1001:4003], pcm[4003:4004], pcm[4004:]]

    tester, _ = run(["hello"], [make_response(pieces)], Mock)

    assert tester.order == ["start", "end"]
    (end,) = tester.ends
    assert end["request_id"] == "req_0"
    assert end["reason"] == REASON_REQUEST_END
    assert tester.starts[0]["request_id"] == "req_0"
    emitted = bytes(tester.audio["req_0"])
    assert emitted == pcm  # nothing lost, duplicated or reordered
    assert end["request_total_audio_duration_ms"] == expected_duration_ms(
        len(emitted)
    )
    assert end["request_total_audio_duration_ms"] == 200
    # Declared audio format: 24 kHz, 16-bit, mono.
    assert tester.frame_formats == {(SAMPLE_RATE, BYTES_PER_SAMPLE, CHANNELS)}


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_event_interval_is_start_to_end_not_ttfb(Mock):
    """request_event_interval_ms spans tts_audio_start -> tts_audio_end,
    measured with a controlled clock."""
    now = {"t": datetime(2026, 1, 1)}

    class FakeDatetime(datetime):
        @classmethod
        def now(cls, tz=None):
            return now["t"]

    def advance(index):
        if index == 1:
            now["t"] += timedelta(milliseconds=1500)

    response = make_response(
        [b"\x01\x00" * 100, b"\x02\x00" * 100], on_chunk=advance
    )
    with patch("ten_ai_base.tts2_http.datetime", FakeDatetime):
        tester, _ = run(["hello"], [response], Mock)

    (end,) = tester.ends
    assert end["request_event_interval_ms"] == 1500
    assert end["request_total_audio_duration_ms"] == expected_duration_ms(400)


# ================ empty / no-audio ================
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_empty_body_ends_once_with_zero_duration(Mock):
    tester, _ = run(["hello"], [make_response([])], Mock)

    assert len(tester.ends) == 1
    assert tester.ends[0]["request_total_audio_duration_ms"] == 0
    assert "req_0" not in tester.audio
    assert tester.errors == []


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_empty_network_chunks_do_not_start_audio(Mock):
    tester, _ = run(
        ["hello"], [make_response([b"", b"", b"\x01\x00" * 50, b""])], Mock
    )

    assert tester.order == ["start", "end"]
    assert len(tester.audio["req_0"]) == 100
    assert tester.ends[0]["request_total_audio_duration_ms"] == (
        expected_duration_ms(100)
    )


# ================ errors ================
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_error_after_partial_audio_ends_once_and_accounts_audio(Mock):
    partial = b"\x05\x00" * 480  # 20 ms
    tester, _ = run(
        ["hello", "next"],
        [
            make_response([partial], fail_after=httpx.ReadError("conn reset")),
            make_response([b"\x01\x00" * 480]),
        ],
        Mock,
    )

    first, second = tester.ends
    assert first["request_id"] == "req_0" and first["reason"] == REASON_ERROR
    assert first["request_total_audio_duration_ms"] == expected_duration_ms(
        len(partial)
    )
    assert len([e for e in tester.ends if e["request_id"] == "req_0"]) == 1
    # Transient transport failure is non-fatal and carries vendor info.
    assert [e["code"] for e in tester.errors] == [NON_FATAL]
    assert tester.errors[0]["vendor_info"]["vendor"] == "breezeblue"
    assert "conn reset" in tester.errors[0]["message"]
    # The next request starts clean: its own audio, no leaked counters.
    assert (
        second["request_id"] == "req_1"
        and second["reason"] == REASON_REQUEST_END
    )
    assert second["request_total_audio_duration_ms"] == expected_duration_ms(
        960
    )
    assert bytes(tester.audio["req_1"]) == b"\x01\x00" * 480


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_rate_limit_before_audio_is_non_fatal_and_recoverable(Mock):
    body = json.dumps(
        {
            "ok": False,
            "code": "RATE_LIMITED",
            "detail": "Slow down",
            "error": "x",
        }
    ).encode()
    tester, client = run(
        ["hello", "again"],
        [
            make_response(status=429, body=body),
            make_response([b"\x01\x00" * 10]),
        ],
        Mock,
    )

    first, second = tester.ends
    assert first["reason"] == REASON_ERROR
    assert first["request_total_audio_duration_ms"] == 0
    assert [e["code"] for e in tester.errors] == [NON_FATAL]
    assert tester.errors[0]["message"] == "Slow down"
    assert second["reason"] == REASON_REQUEST_END
    assert client.stream.call_count == 2  # 429 is not retried


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_insufficient_credits_error_carries_vendor_detail(Mock):
    body = json.dumps(
        {
            "ok": False,
            "code": "BILLING_INSUFFICIENT_CREDITS",
            "detail": "Insufficient credits.",
            "error": "x",
        }
    ).encode()
    tester, _ = run(["hello"], [make_response(status=402, body=body)], Mock)

    assert [e["code"] for e in tester.errors] == [NON_FATAL]
    assert tester.errors[0]["message"] == "Insufficient credits."
    assert len(tester.ends) == 1 and tester.ends[0]["reason"] == REASON_ERROR


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_auth_failure_is_fatal(Mock):
    body = json.dumps(
        {
            "ok": False,
            "code": "AUTH_REQUIRED",
            "detail": "Auth required",
            "error": "x",
        }
    ).encode()
    tester, _ = run(["hello"], [make_response(status=401, body=body)], Mock)

    assert [e["code"] for e in tester.errors] == [FATAL]
    assert tester.errors[0]["vendor_info"]["vendor"] == "breezeblue"


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_unsupported_language_is_a_non_fatal_validation_error(Mock):
    body = json.dumps(
        {
            "ok": False,
            "code": "VALIDATION_ERROR",
            "detail": "Language 'xx' is not supported by this model.",
            "error": "x",
        }
    ).encode()
    tester, _ = run(["hello"], [make_response(status=422, body=body)], Mock)

    assert [e["code"] for e in tester.errors] == [NON_FATAL]
    assert "not supported" in tester.errors[0]["message"]


# ================ HTTP transport ================
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_json_200_is_an_error_never_played_as_pcm(Mock):
    body = json.dumps(
        {"ok": False, "code": "X", "detail": "proxy says no"}
    ).encode()
    response = make_response(
        [b"\x01\x00" * 50],  # would be audio if the guard failed
        body=body,
        headers={"content-type": "application/json"},
    )
    tester, _ = run(["hello"], [response], Mock)

    assert "req_0" not in tester.audio
    assert tester.ends[0]["reason"] == REASON_ERROR
    assert tester.errors[0]["message"] == "proxy says no"


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_transient_502_is_retried_transparently(Mock):
    with patch(
        "breezeblue_tts_python.breezeblue_tts.asyncio.sleep",
        new_callable=AsyncMock,
    ):
        tester, client = run(
            ["hello"],
            [
                make_response(
                    status=502,
                    body=b'{"code":"UPSTREAM_SERVICE_UNAVAILABLE","detail":"later"}',
                ),
                make_response([b"\x01\x00" * 240]),
            ],
            Mock,
        )

    assert tester.errors == []
    assert client.stream.call_count == 2
    assert tester.ends[0]["reason"] == REASON_REQUEST_END
    assert tester.ends[0][
        "request_total_audio_duration_ms"
    ] == expected_duration_ms(480)


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_long_text_is_one_request_with_one_audio_end(Mock):
    """A >1000 char LLM chunk becomes several HTTP calls but a single
    start/end pair with the audio concatenated in order."""
    text = "This is one sentence. " * 100  # 2200 chars
    calls = []

    def respond(*args, **kwargs):
        calls.append(kwargs["json"]["text"])
        return make_response([bytes([len(calls)]) * 480])

    tester, client = run([text], respond, Mock)

    assert len(calls) >= 3 and all(len(t) <= 1000 for t in calls)
    assert "".join(calls) == text
    assert tester.order == ["start", "end"]
    assert len(tester.ends) == 1
    audio = bytes(tester.audio["req_0"])
    assert audio == b"".join(bytes([i + 1]) * 480 for i in range(len(calls)))
    assert tester.ends[0][
        "request_total_audio_duration_ms"
    ] == expected_duration_ms(len(audio))


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_request_wire_contract(Mock):
    """URL, auth header, query and body sent to the vendor."""
    _, client = run(["Hello there"], [make_response([b"\x01\x00" * 10])], Mock)

    (call,) = client.stream.call_args_list
    assert call.args == (
        "POST",
        "https://api.breeze.blue/v1/text-to-speech/voc_test/stream",
    )
    assert call.kwargs["params"] == {"output_format": "pcm"}
    assert call.kwargs["headers"]["xi-api-key"] == "brz_test"
    assert call.kwargs["json"] == {"text": "Hello there", "language_code": "en"}


# ================ cancellation ================
def test_cancel_closes_the_active_response():
    """cancel() closes the in-flight response (not just a flag) and the
    stream ends with a flush instead of an error."""
    from breezeblue_tts_python.config import BreezeBlueTTSConfig
    from breezeblue_tts_python.breezeblue_tts import BreezeBlueTTSClient
    from ten_ai_base.struct import TTS2HttpResponseEventType as Ev

    closed = asyncio.Event()

    async def run_case():
        response = AsyncMock()
        response.status_code = 200
        response.headers = {"content-type": "audio/pcm"}

        async def aiter_bytes():
            yield b"\x01\x00" * 100
            await closed.wait()  # blocks until the response is closed
            raise httpx.ReadError("closed by cancel")

        async def aclose():
            closed.set()

        response.aiter_bytes = aiter_bytes
        response.aclose = aclose
        response.__aenter__ = AsyncMock(return_value=response)
        response.__aexit__ = AsyncMock(return_value=None)

        with patch(
            "breezeblue_tts_python.breezeblue_tts.AsyncClient"
        ) as MockAsyncClient:
            http = AsyncMock()
            http.stream = MagicMock(return_value=response)
            MockAsyncClient.return_value = http
            config = BreezeBlueTTSConfig(params=dict(CONFIG["params"]))
            config.update_params()
            client = BreezeBlueTTSClient(config, MagicMock())

            events = []
            async for item in client.get("hello", "r1"):
                events.append(item)
                if len(events) == 1:
                    asyncio.get_running_loop().call_later(
                        0.05, lambda: asyncio.ensure_future(client.cancel())
                    )
            return events, client

    events, client = asyncio.run(asyncio.wait_for(run_case(), timeout=5))

    assert closed.is_set()
    assert [e for _, e in events] == [Ev.RESPONSE, Ev.FLUSH]
    assert client._response is None  # released on every terminal path
