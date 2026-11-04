import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import asyncio
import json
from unittest.mock import patch, AsyncMock, MagicMock

import pytest

from ten_ai_base.struct import TTS2HttpResponseEventType as Ev


def _make_client(MockAsyncClient, status=200, chunks=(), body=b"", **params):
    from breezeblue_tts_python.config import BreezeBlueTTSConfig
    from breezeblue_tts_python.breezeblue_tts import BreezeBlueTTSClient

    response = AsyncMock()
    response.status_code = status
    response.aread = AsyncMock(return_value=body)

    async def aiter_bytes():
        for c in chunks:
            yield c
            await asyncio.sleep(0)

    response.aiter_bytes = aiter_bytes
    response.__aenter__ = AsyncMock(return_value=response)
    response.__aexit__ = AsyncMock(return_value=None)

    http = AsyncMock()
    http.stream = MagicMock(return_value=response)
    http.aclose = AsyncMock()
    MockAsyncClient.return_value = http

    config = BreezeBlueTTSConfig(
        params={"api_key": "brz_k", "voice_id": "voc_1", **params}
    )
    config.update_params()
    return BreezeBlueTTSClient(config, MagicMock()), http


async def _collect(client, text="hello"):
    return [item async for item in client.get(text, "req1")]


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_streams_pcm_then_end(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 10] * 2)
    events = asyncio.run(_collect(client))

    assert [e for _, e in events] == [Ev.RESPONSE, Ev.RESPONSE, Ev.END]
    assert events[0][0] == b"\x01\x00" * 10

    args, kwargs = http.stream.call_args
    assert args == (
        "POST",
        "https://api.breeze.blue/v1/text-to-speech/voc_1/stream",
    )
    assert kwargs["params"] == {"output_format": "pcm"}
    assert kwargs["headers"]["xi-api-key"] == "brz_k"
    assert kwargs["json"] == {"text": "hello"}


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_odd_byte_chunks_stay_sample_aligned(MockAsyncClient):
    """A 16-bit sample split across network chunks is never emitted torn."""
    client, _ = _make_client(
        MockAsyncClient, chunks=[b"\x01\x02\x03", b"\x04\x05", b"\x06"]
    )
    events = asyncio.run(_collect(client))
    audio = [a for a, e in events if e == Ev.RESPONSE]

    assert all(len(a) % 2 == 0 for a in audio)
    assert b"".join(audio) == b"\x01\x02\x03\x04\x05\x06"
    assert events[-1][1] == Ev.END


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_empty_text_ends_without_request(MockAsyncClient):
    client, http = _make_client(MockAsyncClient)
    events = asyncio.run(_collect(client, "   "))

    assert events == [(None, Ev.END)]
    http.stream.assert_not_called()


@pytest.mark.parametrize("status", [401, 403])
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_auth_errors_map_to_invalid_key(MockAsyncClient, status):
    body = json.dumps(
        {
            "ok": False,
            "code": "AUTH_REQUIRED",
            "detail": "Bad key",
            "error": "x",
        }
    ).encode()
    client, _ = _make_client(MockAsyncClient, status=status, body=body)
    events = asyncio.run(_collect(client))

    assert events == [(b"Bad key", Ev.INVALID_KEY_ERROR)]


@pytest.mark.parametrize(
    "status,code",
    [
        (402, "BILLING_INSUFFICIENT_CREDITS"),
        (422, "VALIDATION_ERROR"),
        (429, "RATE_LIMITED"),
        (503, "UPSTREAM_GENERATION_ERROR"),
    ],
)
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_other_http_errors_map_to_error(MockAsyncClient, status, code):
    body = json.dumps(
        {"ok": False, "code": code, "detail": f"{code} detail", "error": "x"}
    ).encode()
    client, _ = _make_client(MockAsyncClient, status=status, body=body)
    events = asyncio.run(_collect(client))

    assert events == [(f"{code} detail".encode(), Ev.ERROR)]


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_non_json_error_body_is_surfaced(MockAsyncClient):
    client, _ = _make_client(MockAsyncClient, status=502, body=b"Bad Gateway")
    events = asyncio.run(_collect(client))

    assert events == [(b"Bad Gateway", Ev.ERROR)]


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_network_exception_maps_to_error(MockAsyncClient):
    client, http = _make_client(MockAsyncClient)
    http.stream = MagicMock(side_effect=RuntimeError("connection reset"))
    events = asyncio.run(_collect(client))

    assert events == [(b"connection reset", Ev.ERROR)]


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_cancel_mid_stream_flushes(MockAsyncClient):
    client, _ = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 4] * 5)

    async def run():
        events = []
        async for item in client.get("hello", "req1"):
            events.append(item)
            if len(events) == 1:
                await client.cancel()
        return events

    events = asyncio.run(run())
    assert events[0][1] == Ev.RESPONSE
    assert events[-1] == (None, Ev.FLUSH)
    assert all(e != Ev.END for _, e in events)


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_extra_metadata_and_clean(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, model_id="breeze-tts-2")

    assert client.get_extra_metadata() == {
        "model": "breeze-tts-2",
        "voice": "voc_1",
    }
    asyncio.run(client.clean())
    http.aclose.assert_awaited_once()


# ================ text splitting ================
def test_split_text_short_is_untouched():
    from breezeblue_tts_python.breezeblue_tts import split_text

    assert split_text("Hello there.", 1000) == ["Hello there."]


def test_split_text_packs_sentences_under_limit():
    from breezeblue_tts_python.breezeblue_tts import split_text

    text = "One sentence here. " * 100  # 1900 chars
    chunks = split_text(text, 1000)

    assert len(chunks) == 2
    assert all(len(c) <= 1000 for c in chunks)
    assert all(c.rstrip().endswith(".") for c in chunks)
    assert "".join(chunks) == text.rstrip() or "".join(chunks) == text


def test_split_text_overlong_sentence_falls_back_to_words():
    from breezeblue_tts_python.breezeblue_tts import split_text

    text = "word " * 500  # 2500 chars, no sentence end
    chunks = split_text(text, 1000)

    assert all(len(c) <= 1000 for c in chunks)
    assert "".join(chunks).split() == text.split()


def test_split_text_hard_cut_without_spaces():
    from breezeblue_tts_python.breezeblue_tts import split_text

    chunks = split_text("x" * 2500, 1000)

    assert [len(c) for c in chunks] == [1000, 1000, 500]


def test_split_text_cjk_sentences():
    from breezeblue_tts_python.breezeblue_tts import split_text

    text = "你好，这是一个测试。" * 150  # 1500 chars
    chunks = split_text(text, 1000)

    assert len(chunks) == 2 and all(len(c) <= 1000 for c in chunks)
    assert "".join(chunks) == text


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_long_text_is_sent_as_ordered_requests_with_one_end(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 4])
    text = "A short sentence. " * 100  # 1800 chars
    events = asyncio.run(_collect(client, text))

    assert http.stream.call_count == 2
    sent = [c.kwargs["json"]["text"] for c in http.stream.call_args_list]
    assert all(len(t) <= 1000 for t in sent)
    assert "".join(sent) == text
    kinds = [e for _, e in events]
    assert kinds.count(Ev.END) == 1 and kinds[-1] == Ev.END
    assert kinds.count(Ev.RESPONSE) == 2


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_error_in_later_chunk_stops_remaining_chunks(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 4])
    ok = http.stream.return_value
    bad = AsyncMock()
    bad.status_code = 422
    bad.aread = AsyncMock(
        return_value=json.dumps(
            {"code": "VALIDATION_ERROR", "detail": "nope"}
        ).encode()
    )
    bad.__aenter__ = AsyncMock(return_value=bad)
    bad.__aexit__ = AsyncMock(return_value=None)
    http.stream = MagicMock(side_effect=[ok, bad, ok])

    events = asyncio.run(
        _collect(client, "Another sentence. " * 200)
    )  # 3 chunks

    assert http.stream.call_count == 2
    assert events[-1] == (b"nope", Ev.ERROR)
    assert all(e != Ev.END for _, e in events)


def test_max_text_length_from_params_is_not_forwarded():
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    config = BreezeBlueTTSConfig(
        params={"api_key": "k", "voice_id": "v", "max_text_length": 1500}
    )
    config.update_params()

    assert config.max_text_length == 1500
    assert "max_text_length" not in config.build_request_body("hi")
    config.max_text_length = 5000
    with pytest.raises(ValueError, match="max_text_length"):
        config.validate()


# ================ retries and key mapping ================
def _error_response(status, code="UPSTREAM_SERVICE_UNAVAILABLE"):
    r = AsyncMock()
    r.status_code = status
    r.aread = AsyncMock(
        return_value=json.dumps(
            {"code": code, "detail": f"{code} detail"}
        ).encode()
    )
    r.__aenter__ = AsyncMock(return_value=r)
    r.__aexit__ = AsyncMock(return_value=None)
    return r


@pytest.mark.parametrize("status", [502, 503, 504])
@patch("breezeblue_tts_python.breezeblue_tts._RETRY_DELAYS_S", (0, 0))
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_transient_upstream_error_is_retried(MockAsyncClient, status):
    client, http = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 4])
    ok = http.stream.return_value
    http.stream = MagicMock(side_effect=[_error_response(status), ok])
    events = asyncio.run(_collect(client))

    assert http.stream.call_count == 2
    assert [e for _, e in events] == [Ev.RESPONSE, Ev.END]


@patch("breezeblue_tts_python.breezeblue_tts._RETRY_DELAYS_S", (0, 0))
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_persistent_upstream_error_gives_up_after_retries(MockAsyncClient):
    client, http = _make_client(MockAsyncClient)
    http.stream = MagicMock(side_effect=lambda *a, **k: _error_response(502))
    events = asyncio.run(_collect(client))

    assert http.stream.call_count == 3  # first try + 2 retries
    assert events == [(b"UPSTREAM_SERVICE_UNAVAILABLE detail", Ev.ERROR)]


@pytest.mark.parametrize("status", [401, 422, 429])
@patch("breezeblue_tts_python.breezeblue_tts._RETRY_DELAYS_S", (0, 0))
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_client_errors_are_not_retried(MockAsyncClient, status):
    client, http = _make_client(MockAsyncClient)
    http.stream = MagicMock(
        side_effect=lambda *a, **k: _error_response(status, "X")
    )
    asyncio.run(_collect(client))

    assert http.stream.call_count == 1


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_malformed_key_422_maps_to_invalid_key(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, api_key="invalid")
    http.stream = MagicMock(
        side_effect=lambda *a, **k: _error_response(422, "VALIDATION_ERROR")
    )
    events = asyncio.run(_collect(client))

    assert [e for _, e in events] == [Ev.INVALID_KEY_ERROR]


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_422_with_wellformed_key_stays_a_general_error(MockAsyncClient):
    client, http = _make_client(MockAsyncClient, api_key="brz_good")
    http.stream = MagicMock(
        side_effect=lambda *a, **k: _error_response(422, "VALIDATION_ERROR")
    )
    events = asyncio.run(_collect(client))

    assert [e for _, e in events] == [Ev.ERROR]


# ================ cancellation at every stage ================
class BlockedStream:
    """A request that never gets response headers (connect / TTFB wait)."""

    def __init__(self):
        self.entered = asyncio.Event()
        self.interrupted = False

    async def __aenter__(self):
        self.entered.set()
        try:
            await asyncio.sleep(3600)
        except asyncio.CancelledError:
            self.interrupted = True
            raise

    async def __aexit__(self, *exc):
        return False


def test_cancel_while_waiting_for_headers_flushes_and_next_request_works():
    """The next utterance must not wait for the vendor (or the 60 s timeout)
    after a flush that arrived before any response existed."""
    from breezeblue_tts_python.config import BreezeBlueTTSConfig
    from breezeblue_tts_python.breezeblue_tts import BreezeBlueTTSClient

    async def run_case():
        blocked = BlockedStream()
        ok = AsyncMock()
        ok.status_code = 200
        ok.headers = {"content-type": "audio/pcm"}

        async def aiter_bytes():
            yield b"\x01\x00" * 100

        ok.aiter_bytes = aiter_bytes
        ok.__aenter__ = AsyncMock(return_value=ok)
        ok.__aexit__ = AsyncMock(return_value=None)

        with patch(
            "breezeblue_tts_python.breezeblue_tts.AsyncClient"
        ) as MockAsyncClient:
            http = AsyncMock()
            http.stream = MagicMock(side_effect=[blocked, ok])
            MockAsyncClient.return_value = http
            config = BreezeBlueTTSConfig(
                params={"api_key": "brz_k", "voice_id": "voc_1"}
            )
            config.update_params()
            client = BreezeBlueTTSClient(config, MagicMock())

            first = asyncio.ensure_future(_collect(client, "first"))
            await asyncio.wait_for(blocked.entered.wait(), timeout=2)
            assert client._response is None  # nothing to close yet
            start = asyncio.get_running_loop().time()
            await client.cancel()
            first_events = await asyncio.wait_for(first, timeout=2)
            waited = asyncio.get_running_loop().time() - start

            second_events = await asyncio.wait_for(
                _collect(client, "second"), timeout=2
            )
            return blocked, first_events, waited, second_events

    blocked, first_events, waited, second_events = asyncio.run(run_case())

    assert blocked.interrupted
    assert first_events == [(None, Ev.FLUSH)]
    assert waited < 1.0
    assert [e for _, e in second_events] == [Ev.RESPONSE, Ev.END]


@patch("breezeblue_tts_python.breezeblue_tts._RETRY_DELAYS_S", (30, 30))
@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_cancel_during_retry_backoff_wakes_up_and_does_not_reopen(
    MockAsyncClient,
):
    client, http = _make_client(MockAsyncClient)
    http.stream = MagicMock(side_effect=lambda *a, **k: _error_response(502))

    async def run_case():
        task = asyncio.ensure_future(_collect(client))
        for _ in range(100):  # wait until the first attempt has failed
            await asyncio.sleep(0.01)
            if http.stream.call_count >= 1:
                break
        await asyncio.sleep(0.05)  # now inside the 30 s back-off
        start = asyncio.get_running_loop().time()
        await client.cancel()
        events = await asyncio.wait_for(task, timeout=2)
        return events, asyncio.get_running_loop().time() - start

    events, waited = asyncio.run(run_case())

    assert events == [(None, Ev.FLUSH)]
    assert waited < 1.0  # did not sit out the 30 s back-off
    assert http.stream.call_count == 1  # and did not retry after the flush


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_cancel_between_split_pieces_does_not_open_next_request(
    MockAsyncClient,
):
    client, http = _make_client(MockAsyncClient, chunks=[b"\x01\x00" * 10])
    text = "A short sentence. " * 100  # 1800 chars -> 2 requests

    async def run_case():
        events = []
        async for item in client.get(text, "r1"):
            events.append(item)
            if len(events) == 1:
                await client.cancel()
        return events

    events = asyncio.run(run_case())

    assert http.stream.call_count == 1
    assert events[-1] == (None, Ev.FLUSH)
    assert all(e != Ev.END for _, e in events)


@patch("breezeblue_tts_python.breezeblue_tts.AsyncClient")
def test_external_task_cancellation_is_not_swallowed(MockAsyncClient):
    """Only cancel() turns a CancelledError into a flush; a real task
    cancellation (shutdown) must propagate."""
    client, http = _make_client(MockAsyncClient)
    http.stream = MagicMock(return_value=BlockedStream())

    async def run_case():
        task = asyncio.ensure_future(_collect(client))
        await asyncio.sleep(0.05)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    asyncio.run(run_case())


# ================ credential redaction ================
@pytest.mark.parametrize(
    "header", ["xi-api-key", "XI-API-KEY", "Authorization", "authorization"]
)
def test_to_str_redacts_credentials_sent_as_headers(header):
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    secret = "brz_HEADER_SECRET_0123456789abcdef"
    config = BreezeBlueTTSConfig(
        headers={header: f"Bearer {secret}", "Content-Type": "audio/pcm"},
        params={"voice_id": "voc_1"},
    )
    config.update_params()

    text = config.to_str()
    assert secret not in text
    assert "audio/pcm" in text  # unrelated headers stay readable
    # The original object is untouched (redaction works on a copy).
    assert config.headers[header].endswith(secret)
    # sensitive_handling=False is the explicit opt-out.
    assert secret in config.to_str(sensitive_handling=False)


def test_to_str_redacts_params_and_headers_together():
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    config = BreezeBlueTTSConfig(
        headers={"xi-api-key": "brz_in_header_000000000000"},
        params={"api_key": "brz_in_params_000000000000", "voice_id": "v"},
    )
    config.update_params()

    text = config.to_str()
    assert "brz_in_header_000000000000" not in text
    assert "brz_in_params_000000000000" not in text
