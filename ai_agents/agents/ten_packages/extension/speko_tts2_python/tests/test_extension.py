import json
from unittest.mock import AsyncMock, MagicMock

import pytest
from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.struct import TTSTextInput

from speko_tts2_python.client import SpekoRouterError
from speko_tts2_python.config import SpekoTTS2Config
from speko_tts2_python.extension import SpekoTTS2Extension


def make_extension():
    extension = SpekoTTS2Extension("speko")
    extension.config = SpekoTTS2Config(api_key="test-key")
    extension.ten_env = MagicMock()
    for name in (
        "on_disconnected",
        "send_tts_audio_start",
        "send_tts_audio_end",
        "send_tts_error",
        "send_usage_metrics",
        "_flush_recorder",
    ):
        setattr(extension, name, AsyncMock())
    # Wrap the actual base method to verify it releases the request slot.
    extension.finish_request = AsyncMock(wraps=extension.finish_request)
    return extension


def text(value="hello", request_id="req", final=False):
    return TTSTextInput(text=value, request_id=request_id, text_input_end=final)


@pytest.mark.asyncio
async def test_intermediate_error_keeps_request_open_and_next_chunk_runs():
    extension = make_extension()
    extension._processing_request_id = "req"
    client = MagicMock()
    client.close = AsyncMock()

    async def fail_once(*_):
        extension.client = client
        raise SpekoRouterError("provider_error", "temporary", retryable=True)

    extension._stream_text = AsyncMock(side_effect=fail_once)
    await extension.request_tts(text())
    extension.finish_request.assert_not_awaited()
    extension.send_tts_audio_end.assert_not_awaited()
    assert extension._processing_request_id == "req"
    client.close.assert_awaited_once_with(drain=False)
    extension._stream_text.side_effect = None
    await extension.request_tts(text(" next", final=True))
    extension._stream_text.assert_awaited_with(" next", "req")
    extension.finish_request.assert_awaited_once()
    # The earlier chunk's text is missing, so the request ends as an error.
    assert (
        extension.finish_request.await_args.kwargs["reason"]
        == TTSAudioEndReason.ERROR
    )
    assert extension._processing_request_id is None


@pytest.mark.asyncio
async def test_close_error_emits_error_end_and_releases_next_request():
    extension = make_extension()
    await extension._begin_request(text())
    extension._processing_request_id = "req"
    client = MagicMock(usage={"characters": 5})
    client.close = AsyncMock(
        side_effect=SpekoRouterError(
            "provider_error", "settlement failed", retryable=True
        )
    )
    extension.client = client
    await extension._finalize_request(TTSAudioEndReason.REQUEST_END)
    extension.send_tts_audio_end.assert_awaited_once()
    assert (
        extension.send_tts_audio_end.await_args.kwargs["reason"]
        == TTSAudioEndReason.ERROR
    )
    assert extension.send_usage_metrics.await_args.kwargs["extra_metadata"][
        "router_usage"
    ] == {"characters": 5}
    extension._flush_recorder.assert_awaited_once_with("req")
    extension.finish_request.assert_awaited_once()
    assert extension._processing_request_id is None
    assert extension.client is None
    await extension._finalize_request(TTSAudioEndReason.REQUEST_END)
    extension.finish_request.assert_awaited_once()
    extension._stream_text = AsyncMock()
    await extension.request_tts(text("next", "next", True))
    assert extension.finish_request.await_count == 2


@pytest.mark.asyncio
async def test_final_chunk_error_finishes_once():
    extension = make_extension()
    extension._stream_text = AsyncMock(side_effect=OSError("closed"))
    await extension.request_tts(text(final=True))
    extension.finish_request.assert_awaited_once()
    assert (
        extension.finish_request.await_args.kwargs["reason"]
        == TTSAudioEndReason.ERROR
    )


@pytest.mark.asyncio
async def test_whitespace_and_output_metrics():
    extension = make_extension()
    client = MagicMock()

    async def stream(value):
        sent.append(value)
        if False:
            yield

    sent = []
    client.stream_text = stream
    extension._ensure_client = AsyncMock(return_value=client)
    extension.metrics_add_output_characters = MagicMock()
    extension.metrics_add_input_characters = MagicMock()
    for value in ("hello", " ", "\n\t", " world "):
        await extension.request_tts(text(value))
    # Whitespace-only chunks are not sent to the Router at all.
    assert sent == ["hello", " world "]
    assert (
        sum(
            call.args[0]
            for call in extension.metrics_add_output_characters.call_args_list
        )
        == 12
    )
    extension.send_tts_error.assert_not_awaited()
    extension.metrics_add_input_characters.assert_not_called()


@pytest.mark.asyncio
async def test_permanent_denial_does_not_reopen_client():
    extension = make_extension()
    extension._stream_text = AsyncMock(
        side_effect=SpekoRouterError("insufficient_credit", "no credit")
    )
    await extension.request_tts(text())
    await extension.request_tts(text("next", final=True))
    assert extension._stream_text.await_count == 1
    extension.finish_request.assert_awaited_once()


@pytest.mark.asyncio
async def test_cancel_cleanup_even_if_client_fails():
    extension = make_extension()
    await extension._begin_request(text())
    client = MagicMock()
    client.cancel = AsyncMock(side_effect=OSError("closed"))
    extension.client = client
    await extension.cancel_tts()
    assert extension.client is None
    extension._flush_recorder.assert_awaited_once_with("req")
    assert (
        extension.send_tts_audio_end.await_args.kwargs["reason"]
        == TTSAudioEndReason.INTERRUPTED
    )
    extension.finish_request.assert_not_awaited()


@pytest.mark.asyncio
async def test_recorder_failure_still_releases_request():
    extension = make_extension()
    await extension._begin_request(text())
    extension._processing_request_id = "req"
    extension._flush_recorder.side_effect = OSError("disk")
    with pytest.raises(OSError):
        await extension._finalize_request(TTSAudioEndReason.REQUEST_END)
    extension.finish_request.assert_awaited_once()
    assert extension._processing_request_id is None


def test_vendor_metadata():
    extension = make_extension()
    assert extension.vendor_metadata()["key"] == "test-key"
    assert "api_key" not in extension.vendor_metadata()
    assert extension.vendor_metadata()["language"] == "en"


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "code", ["provider_error", "relay_error", "invalid_request"]
)
async def test_nonretryable_chunk_error_allows_new_text_without_replay(code):
    extension = make_extension()
    extension._stream_text = AsyncMock(
        side_effect=SpekoRouterError(code, "chunk failed", retryable=False)
    )
    await extension.request_tts(text("rejected"))
    extension.finish_request.assert_not_awaited()
    extension._stream_text.side_effect = None
    await extension.request_tts(text("valid next chunk", final=True))
    assert [
        call.args[0] for call in extension._stream_text.await_args_list
    ] == ["rejected", "valid next chunk"]
    extension.send_tts_error.assert_awaited_once()
    await extension.request_tts(text("new", "next", True))
    extension._stream_text.assert_awaited_with("new", "next")
    assert extension.finish_request.await_count == 2


@pytest.mark.asyncio
async def test_flush_during_close_emits_only_interruption():
    import asyncio

    extension = make_extension()
    await extension._begin_request(text())
    entered = asyncio.Event()

    async def close(**_kwargs):
        entered.set()
        await asyncio.Future()

    client = MagicMock(usage={})
    client.close = AsyncMock(side_effect=close)
    client.cancel = AsyncMock()
    extension.client = client
    finalizing = asyncio.create_task(
        extension._finalize_request(TTSAudioEndReason.REQUEST_END)
    )
    await entered.wait()
    finalizing.cancel()
    await extension.cancel_tts()
    with pytest.raises(asyncio.CancelledError):
        await finalizing
    extension.send_tts_audio_end.assert_awaited_once()
    assert (
        extension.send_tts_audio_end.await_args.kwargs["reason"]
        == TTSAudioEndReason.INTERRUPTED
    )
    extension._flush_recorder.assert_awaited_once_with("req")
    extension.finish_request.assert_not_awaited()


def test_explicit_route_removes_merged_auto_objective():
    config = SpekoTTS2Config(
        api_key="test-key",
        params={
            "routing": {
                "mode": "explicit",
                "objective": "balanced",
                "provider": "deepgram",
                "model": "nova-3",
            }
        },
    )
    config.update_params()
    assert config.routing == {
        "mode": "explicit",
        "provider": "deepgram",
        "model": "nova-3",
    }


@pytest.mark.asyncio
async def test_shutdown_joins_reentrant_cancel_without_duplicate_end():
    import asyncio

    extension = make_extension()
    await extension._begin_request(text())
    entered = asyncio.Event()

    async def cancel():
        entered.set()
        await asyncio.Future()

    client = MagicMock(cancel=AsyncMock(side_effect=cancel))
    extension.client = client
    cancelling = asyncio.create_task(extension.cancel_tts())
    await entered.wait()
    extension._stopped = True
    await extension.cancel_tts()
    cancelling.cancel()
    await cancelling
    client.cancel.assert_awaited_once()
    extension._flush_recorder.assert_awaited_once_with("req")
    extension.send_tts_audio_end.assert_not_awaited()


@pytest.mark.asyncio
async def test_old_close_does_not_clear_a_new_requests_client_or_usage():
    import asyncio

    extension = make_extension()
    extension.current_request_id = "old"
    entered, release = asyncio.Event(), asyncio.Event()

    async def close(**_kwargs):
        entered.set()
        await release.wait()

    extension.client = MagicMock(
        close=AsyncMock(side_effect=close), usage={"characters": 1}
    )
    closing = asyncio.create_task(extension._close_client())
    await entered.wait()
    replacement = MagicMock()
    extension.client = replacement
    extension.current_request_id = "new"
    extension._router_usage = {"characters": 2}
    release.set()
    await closing
    assert extension.client is replacement
    assert extension._router_usage == {"characters": 2}
    extension.on_disconnected.assert_not_awaited()


@pytest.mark.asyncio
@pytest.mark.parametrize("late_error", [False, True])
async def test_base_flush_cancels_active_stream_and_allows_next_request(
    late_error,
):
    import asyncio
    from ten_ai_base.tts2 import RequestState

    extension = make_extension()
    extension._processing_request_id = "req"
    extension.request_states["req"] = RequestState.PROCESSING
    started = asyncio.Event()
    cancelled = asyncio.Event()
    client = MagicMock()
    client.cancel = AsyncMock()

    async def stream(*_):
        extension.client = client
        started.set()
        try:
            await asyncio.Event().wait()
        except asyncio.CancelledError:
            if late_error:
                # A receive can complete concurrently with task cancellation.
                raise OSError("socket closed during flush") from None
            raise
        finally:
            cancelled.set()

    extension._stream_text = AsyncMock(side_effect=stream)
    request = asyncio.create_task(extension.request_tts(text()))
    await started.wait()
    await extension._flush_input_items()
    if late_error:
        await request
    else:
        with pytest.raises(asyncio.CancelledError):
            await request
    assert cancelled.is_set()
    assert extension.current_task is None
    assert extension._processing_request_id is None
    extension.send_tts_error.assert_not_awaited()
    extension.finish_request.assert_not_awaited()
    extension.send_tts_audio_end.assert_awaited_once()
    assert (
        extension.send_tts_audio_end.await_args.kwargs["reason"]
        == TTSAudioEndReason.INTERRUPTED
    )
    client.cancel.assert_awaited_once()

    extension._stream_text.side_effect = None
    await extension.request_tts(text("next", "next", True))
    extension.finish_request.assert_awaited_once()


@pytest.mark.parametrize(
    "code",
    [
        "authentication_failed",
        "insufficient_credit",
        "route_not_found",
        "capability_unsupported",
    ],
)
def test_terminal_error_classification_matches_latch(code):
    from ten_ai_base.message import ModuleErrorCode

    extension = make_extension()
    error = SpekoRouterError(code, "terminal")
    result = extension._make_module_error(error)
    assert str(result.code) == ModuleErrorCode.FATAL_ERROR.value
    assert extension._permanent_error is error
    assert result.vendor_info.code == code


@pytest.mark.asyncio
@pytest.mark.parametrize("close_fails", [False, True])
async def test_replaced_request_recorder_is_flushed_even_if_close_fails(
    close_fails,
):
    extension = SpekoTTS2Extension("speko")
    extension.config = SpekoTTS2Config(api_key="key")
    extension.ten_env = MagicMock()
    extension.on_disconnected = AsyncMock()
    extension.current_request_id = "previous"
    recorder = MagicMock(flush=AsyncMock())
    extension._recorders["previous"] = recorder
    extension.client = MagicMock(
        close=AsyncMock(side_effect=OSError("close") if close_fails else None)
    )
    if close_fails:
        with pytest.raises(OSError):
            await extension._begin_request(text(request_id="next"))
    else:
        await extension._begin_request(text(request_id="next"))
    recorder.flush.assert_awaited_once()
    assert "previous" not in extension._recorders


@pytest.mark.asyncio
async def test_connection_event_redacts_credentials_and_url():
    extension = SpekoTTS2Extension("speko")
    extension.config = SpekoTTS2Config(
        api_key="test-secret-key-long",
        base_url="https://user:password@example.com?token=private#secret",
    )
    extension.ten_env = MagicMock(send_data=AsyncMock())
    await extension.on_connecting()
    data = extension.ten_env.send_data.await_args.args[0]
    raw, _ = data.get_property_to_json("")
    vendor = json.loads(raw)["metadata"]["vendor_metadata"]
    assert vendor["base_url"] == "https://example.com"
    assert "key" in vendor and "api_key" not in vendor
    for secret in ("test-secret-key-long", "password", "private", "#secret"):
        assert secret not in raw + extension.config.to_str()


@pytest.mark.asyncio
async def test_final_chunk_error_reports_one_disconnect():
    extension = make_extension()
    await extension._begin_request(text())
    extension._processing_request_id = "req"
    client = MagicMock(usage={"characters": 3})
    client.close = AsyncMock()

    async def fail(*_):
        extension.client = client
        raise SpekoRouterError("provider_error", "boom", retryable=True)

    extension._stream_text = AsyncMock(side_effect=fail)
    await extension.request_tts(text(final=True))
    extension.on_disconnected.assert_awaited_once()
    assert extension.on_disconnected.await_args.kwargs["code"] != 0
    client.close.assert_awaited_once_with(drain=False)
    extension.finish_request.assert_awaited_once()
    assert extension.send_usage_metrics.await_args.kwargs["extra_metadata"][
        "router_usage"
    ] == {"characters": 3}


@pytest.mark.asyncio
async def test_close_failure_after_stop_does_not_finish_as_success():
    extension = make_extension()
    await extension._begin_request(text())
    extension._processing_request_id = "req"
    client = MagicMock(usage={})

    async def close(**_kwargs):
        extension._stopped = True
        raise OSError("closed during shutdown")

    client.close = AsyncMock(side_effect=close)
    extension.client = client
    await extension._finalize_request(TTSAudioEndReason.REQUEST_END)
    extension.send_tts_audio_end.assert_not_awaited()
    extension.send_tts_error.assert_not_awaited()
    extension.finish_request.assert_awaited_once()
    assert (
        extension.finish_request.await_args.kwargs["reason"]
        == TTSAudioEndReason.INTERRUPTED
    )


class _ClosingSocket:
    """A Router socket that fails during the close handshake."""

    def __init__(self, failure):
        self.failure = failure
        self.sent = []
        self.closed = False

    async def send(self, value):
        self.sent.append(value)

    async def recv(self):
        if self.failure == "closed":
            from websockets.exceptions import ConnectionClosedError

            raise ConnectionClosedError(None, None)
        return json.dumps(
            {
                "type": "error",
                "error": {"code": "provider_error", "message": "late"},
            }
        )

    async def close(self):
        self.closed = True


def _ready_client(socket):
    from speko_tts2_python.client import SpekoTTSClient

    client = SpekoTTSClient(
        api_key="key",
        base_url="https://router.speko.dev",
        configure={},
        ready_timeout_sec=1,
        receive_timeout_sec=1,
    )
    client._ws = socket
    client._ready = True
    return client


@pytest.mark.asyncio
@pytest.mark.parametrize("failure", ["closed", "error"])
async def test_stop_never_raises_when_router_close_fails(failure):
    extension = SpekoTTS2Extension("speko")
    extension.config = SpekoTTS2Config(api_key="key")
    extension.ten_env = MagicMock(
        send_data=AsyncMock(), send_audio_frame=AsyncMock()
    )
    socket = _ClosingSocket(failure)
    extension.client = _ready_client(socket)
    extension.current_request_id = "req"
    await extension.on_stop(extension.ten_env)
    assert extension.client is None
    assert socket.closed


@pytest.mark.asyncio
async def test_stop_logs_instead_of_raising_when_close_raises():
    extension = SpekoTTS2Extension("speko")
    extension.config = SpekoTTS2Config(api_key="key")
    extension.ten_env = MagicMock(send_data=AsyncMock())
    extension.client = MagicMock(
        close=AsyncMock(side_effect=SpekoRouterError("relay_error", "x"))
    )
    extension.current_request_id = "req"
    extension._recorders["req"] = MagicMock(
        flush=AsyncMock(side_effect=OSError("disk"))
    )
    await extension.on_stop(extension.ten_env)
    assert extension.ten_env.log_warn.call_count == 2
    assert not extension._recorders


@pytest.mark.asyncio
@pytest.mark.parametrize("failure", ["closed", "error"])
async def test_drain_treats_closed_socket_as_closed_and_surfaces_errors(
    failure,
):
    socket = _ClosingSocket(failure)
    client = _ready_client(socket)
    if failure == "closed":
        await client.close(drain=True)
    else:
        with pytest.raises(SpekoRouterError):
            await client.close(drain=True)
    assert socket.closed
    assert not client.is_ready


@pytest.mark.asyncio
async def test_drain_is_bounded_while_audio_keeps_arriving():
    import asyncio
    from unittest.mock import patch

    class StreamingSocket(_ClosingSocket):
        async def recv(self):
            await asyncio.sleep(0.001)
            return b"\x00\x00"

    socket = StreamingSocket("audio")
    client = _ready_client(socket)
    with patch("speko_tts2_python.client.DRAIN_TIMEOUT_SEC", 0.05):
        await asyncio.wait_for(client.close(drain=True), 1)
    assert socket.closed
