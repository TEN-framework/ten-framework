#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from dataclasses import dataclass, field
import os
import re
import time
import traceback
from typing import Awaitable

from pydantic import ValidationError

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension, RequestState
from ten_runtime import AsyncTenEnv

from .config import DEFAULT_SAMPLE_RATE, KalpaTTSConfig
from .kalpa_tts import OVERLOADED, KalpaTTSClient, KalpaTTSError
from .spacing import SentenceSpacing

# Kalpa's limit on the text of one utterance. A longer request continues in a
# second utterance.
MAX_UTTERANCE_CHARS = 8000
# Utterances in one request. Kalpa queues at most 20 behind the one it is
# speaking, so a request that needs more is refused.
MAX_UTTERANCES = 20
# Times one request is sent again after its connection dropped, or Kalpa
# refused it as overloaded, before any of its audio arrived.
MAX_REPLAYS = 2
# Failed and interrupted requests remembered, to drop input that arrives late.
MAX_ENDED_REQUESTS = 64
# How long Kalpa may send nothing for an utterance it was asked to speak. A
# response that never ends would otherwise hold up every later request.
RESPONSE_TIMEOUT_S = 30.0

ERROR = TTSAudioEndReason.ERROR
INTERRUPTED = TTSAudioEndReason.INTERRUPTED
REQUEST_END = TTSAudioEndReason.REQUEST_END


def _split(text: str, limit: int) -> list[str]:
    """Cut text longer than one utterance at the last space before the limit."""
    pieces = []
    while len(text) > limit:
        cut = text.rfind(" ", 0, limit) + 1 or limit
        pieces.append(text[:cut])
        text = text[cut:]
    return pieces + [text]


@dataclass
class _Request:
    """One TEN request: the text sent for it and the audio received."""

    request_id: str
    # Every message of the request, `(text, flush)`, in order: kept so the
    # request can be sent again on a new connection. `sent` of them went out
    # on the current connection and `counted` were reported as usage.
    ops: list[tuple[str, bool]] = field(default_factory=list)
    sent: int = 0
    counted: int = 0
    # Characters in the utterance that is still open.
    open_chars: int = 0
    spacing: SentenceSpacing = field(default_factory=SentenceSpacing)
    # Utterances ended, how many of those ends the current connection has
    # had, and how many utterances Kalpa has finished speaking.
    flushes: int = 0
    flushes_sent: int = 0
    done: int = 0
    # Runs out when Kalpa owes audio and sends nothing.
    deadline: asyncio.TimerHandle | None = None
    input_ended: bool = False
    replays: int = 0
    # Monotonic times of the first text sent, of the first audio received
    # and of `tts_audio_start`.
    send_started_at: float | None = None
    first_audio_at: float | None = None
    audio_started_at: float | None = None
    # Bytes sent downstream, and the bytes of a sample that is not whole yet.
    audio_bytes: int = 0
    carry: bytes = b""
    # Set once the request has ended. Only `end_task` emits for it afterwards.
    closed: bool = False
    end_reason: TTSAudioEndReason | None = None
    end_task: asyncio.Task | None = None


class KalpaTTSExtension(AsyncTTS2BaseExtension):
    """Kalpa text-to-speech over the streaming WebSocket.

    Each `tts_text_input` is forwarded as it arrives. A request is one Kalpa
    utterance, which Kalpa starts speaking at its first complete sentences;
    `text_input_end` ends it. A flush drops the connection and opens a new
    one, so neither text nor audio of the interrupted request can reach the
    next one.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: KalpaTTSConfig | None = None
        self.client: KalpaTTSClient | None = None
        self._request: _Request | None = None
        # Requests that failed or were interrupted, by id, oldest first.
        self._ended: dict[str, _Request] = {}
        self._recorders: dict[str, PCMWriter] = {}
        # One sender at a time keeps a request's messages in order when a
        # reconnect sends them again while new input is arriving.
        self._send_lock = asyncio.Lock()
        # The task emitting the last events of the request that ended last.
        self._ending: asyncio.Task | None = None
        self._init_error: str | None = None
        self._stopped = False

    # ── Lifecycle ──────────────────────────────────────────────────

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json, _ = await ten_env.get_property_to_json("")
            config = KalpaTTSConfig.model_validate_json(config_json or "{}")
            config.update_params()
            ten_env.log_info(
                f"config: {config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            config.check()
            self.config = config
            # Connect now, so the session is ready before the first text.
            self._start_client()
        except ValidationError as exc:
            # A validation error quotes the rejected value, which may be the
            # key, so only its locations and messages are reported.
            await self._fail_init(
                "; ".join(
                    f"{'.'.join(map(str, error['loc']))}: {error['msg']}"
                    for error in exc.errors(
                        include_input=False, include_url=False
                    )
                )
            )
        except Exception as exc:
            await self._fail_init(str(exc))

    async def _fail_init(self, message: str) -> None:
        self._init_error = f"Initialization failed: {message}"
        self.ten_env.log_error(f"on_init failed: {message}")
        await self.send_tts_error(
            request_id="",
            error=self._module_error(
                KalpaTTSError("invalid_config", self._init_error), fatal=True
            ),
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self._stopped = True
        request, self._request = self._request, None
        if request is not None:
            request.closed = True
            self._watch(request)
        client, self.client = self.client, None
        if client is not None:
            client.stop()
        if self._ending is not None and not self._ending.done():
            # A request that was already ending gets its last events before
            # the base class forgets it, not after.
            await asyncio.shield(self._ending)
        for request_id in list(self._recorders):
            await self._close_recorder(request_id)
        await super().on_stop(ten_env)

    def vendor(self) -> str:
        return "kalpa"

    def vendor_metadata(self) -> dict:
        if self.config is None:
            return {}
        return {
            "url": self.config.safe_url,
            "model": self.config.model,
            "voice_id": self.config.voice_id,
        }

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else DEFAULT_SAMPLE_RATE

    def _start_client(self) -> None:
        self.client = KalpaTTSClient(self.config, self.ten_env, self)
        self.client.start()

    # ── Input ──────────────────────────────────────────────────────

    async def request_tts(self, t: TTSTextInput) -> None:
        # A flush cancels `current_task`. The work runs in it, so a flush
        # also ends a wait for a session or for a slow socket, and the next
        # request is not held up behind it.
        task = asyncio.create_task(self._request_tts(t))
        self.current_task = task
        try:
            await task
        finally:
            if self.current_task is task:
                self.current_task = None

    async def _request_tts(self, t: TTSTextInput) -> None:
        request_id = t.request_id
        if self._stopped:
            # The base class has dropped its request state; anything emitted
            # now would go out without the request's metadata.
            return
        try:
            if self._ending is not None and not self._ending.done():
                # The request that ended last emits its last events first:
                # the session that replaces its own is started among them,
                # and only then is the base class told that it is over.
                await asyncio.shield(self._ending)
            ended = self._ended.get(request_id)
            if ended is not None:
                # More input for a request that failed or was flushed.
                # Nothing of it is spoken. The base class has made it the
                # active request again, so it is released for the next one.
                if t.text_input_end:
                    # Its text is over, so the id can name a new request.
                    del self._ended[request_id]
                await self.finish_request(request_id, reason=ended.end_reason)
                return

            request = self._request
            if request is None or request.request_id != request_id:
                request = await self._begin_request(request_id)
            # Set before anything can end the request: an id whose text has
            # ended is free to name a new request.
            request.input_ended = request.input_ended or t.text_input_end
            client = self.client
            failed = (
                KalpaTTSError(
                    "invalid_config",
                    self._init_error or "The extension is stopped",
                    permanent=True,
                )
                if client is None
                else client.failed
            )
            if failed is not None and failed.permanent:
                await self._end_request(
                    request, ERROR, self._module_error(failed, fatal=True)
                )
                return
            if failed is not None:
                # The connection was given up on after repeated failures.
                # A new request is a reason to try again. The old client is
                # stopped first: its report of the failure may still be on
                # its way, and it must not reach this request.
                client.stop()
                self._start_client()

            try:
                self._journal(request, t.text or "", t.text_input_end)
            except KalpaTTSError as error:
                await self._fail_request(request, error)
                return
            if request.open_chars and t.text_input_end:
                self._journal_flush(request)
            elif request.open_chars:
                # Text that follows `text_input_end` under the same id,
                # while the request is still open, is an agent's next
                # message of the turn: the request stays open until that
                # text ends as well.
                request.input_ended = False
            await self._drain(request)
            await self._complete_if_done(request)
        except Exception:
            self.ten_env.log_error(
                f"request_tts failed for {request_id}: "
                f"{traceback.format_exc()}"
            )
            request = self._request
            if request is not None and request.request_id == request_id:
                await self._fail_request(
                    request,
                    KalpaTTSError("internal_error", "request_tts failed"),
                )

    async def finish_request(
        self,
        request_id: str,
        reason: TTSAudioEndReason = REQUEST_END,
        error: ModuleError | None = None,
    ) -> None:
        # ten_ai_base 0.7.65 puts the next request's buffered text back
        # behind text that was queued while it waited, although it arrived
        # first. It is moved back to the front.
        buffered = [
            item for items in self._pending_messages.values() for item in items
        ]
        await super().finish_request(request_id, reason=reason, error=error)
        held = {id(item) for item in buffered}
        queue = self.input_queue._queue  # pylint: disable=protected-access
        released = [item for item in queue if id(item) in held]
        if released:
            later = [item for item in queue if id(item) not in held]
            queue.clear()
            queue.extend(released + later)

    async def _begin_request(self, request_id: str) -> _Request:
        if self._request is not None:
            # The base class starts a request only after the previous one
            # ended. If one is still open, it is interrupted on a session of
            # its own, so that none of it reaches this request.
            await self._end_request(
                self._request, INTERRUPTED, replace_client=True
            )
        request = _Request(request_id)
        self._request = request
        # An agent may speak again under an id that has ended. The base class
        # keeps that id as completed, no longer as the one in progress, and
        # a flush skips such a request, so the id is made active again.
        if self.request_states.get(request_id) in (
            None,
            RequestState.COMPLETED,
        ):
            self.request_states[request_id] = RequestState.PROCESSING
        self._processing_request_id = request_id
        if self.config is not None and self.config.dump:
            os.makedirs(self.config.dump_path, exist_ok=True)
            # The id comes from another extension: it must not name a path.
            name = re.sub(r"[^\w.-]", "_", request_id)
            self._recorders[request_id] = PCMWriter(
                os.path.join(self.config.dump_path, f"kalpa_dump_{name}.pcm")
            )
        return request

    def _journal(self, request: _Request, text: str, last: bool) -> None:
        # Kalpa appends each message to the utterance, so the text is sent
        # as it was written, except that the space after a sentence moves
        # from the front of the next piece to the end of this one (see
        # `spacing.py`). Whitespace before the first word would open an
        # utterance with nothing to say.
        text = request.spacing.apply(text, last)
        if not text or (not request.open_chars and not text.strip()):
            return
        for piece in _split(text, MAX_UTTERANCE_CHARS):
            if request.open_chars + len(piece) > MAX_UTTERANCE_CHARS:
                # `flushes` utterances have ended and one is open. The
                # piece would start one more.
                if request.flushes + 2 > MAX_UTTERANCES:
                    limit = MAX_UTTERANCES * MAX_UTTERANCE_CHARS
                    raise KalpaTTSError(
                        "request_too_long",
                        f"A request may hold at most {limit:,} characters",
                    )
                self._journal_flush(request)
            request.ops.append((piece, False))
            request.open_chars += len(piece)

    @staticmethod
    def _journal_flush(request: _Request) -> None:
        request.ops.append(("", True))
        request.flushes += 1
        request.open_chars = 0

    async def _drain(self, request: _Request) -> None:
        """Send the request's messages that the connection has not had yet."""
        async with self._send_lock:
            while not request.closed and request.sent < len(request.ops):
                client = self.client
                if client is None or not await client.wait_ready():
                    # The client gave up or was stopped; whoever caused that
                    # ends the request.
                    return
                if request.closed or request.sent >= len(request.ops):
                    return
                text, flush = request.ops[request.sent]
                replays = request.replays
                if text.strip() and request.send_started_at is None:
                    # The start of TTFB. It is set before the send, which
                    # can still be waiting on a slow socket when the audio
                    # it asked for arrives.
                    request.send_started_at = time.monotonic()
                try:
                    await client.send_text(text, flush)
                except KalpaTTSError:
                    # The connection dropped. The client reports it, and the
                    # request is sent again or failed there.
                    return
                if request.closed:
                    return
                if request.replays != replays:
                    # The connection dropped while this was being sent, and
                    # the request starts over on the next one.
                    continue
                request.sent += 1
                if flush:
                    request.flushes_sent += 1
                    self._watch(request)
                if request.sent > request.counted:
                    # A request that is sent again is counted once.
                    request.counted = request.sent
                    if text.strip():
                        self.metrics_add_output_characters(len(text))
                        await self.send_tts_request_metrics(
                            request_id=request.request_id, request_text=text
                        )

    # ── Events from the client ─────────────────────────────────────

    async def on_session_ready(self) -> None:
        request = self._request
        if request is not None and not request.closed:
            await self._drain(request)

    async def on_audio(self, pcm: bytes, received_at: float) -> None:
        request = self._request
        if request is None or request.closed:
            return
        self._watch(request)
        if request.first_audio_at is None:
            request.first_audio_at = received_at
        # Kalpa's chunks may end inside a sample. Only whole samples go on,
        # so the dump and the reported duration are those of the audio sent
        # downstream, also when the request ends on half a sample.
        pcm = request.carry + pcm
        whole = len(pcm) - len(pcm) % self._sample_bytes()
        request.carry, pcm = pcm[whole:], pcm[:whole]
        if not pcm:
            return

        if request.audio_started_at is None:
            request.audio_started_at = time.monotonic()
            await self.send_tts_audio_start(request_id=request.request_id)
            if request.send_started_at is not None and not request.closed:
                elapsed = request.first_audio_at - request.send_started_at
                await self.send_tts_ttfb_metrics(
                    request_id=request.request_id,
                    ttfb_ms=max(0, round(elapsed * 1000)),
                )
        recorder = self._recorders.get(request.request_id)
        if recorder is not None and not request.closed:
            await recorder.write(pcm)
        if request.closed:
            return
        # Counted and sent with nothing in between.
        request.audio_bytes += len(pcm)
        self.metrics_add_recv_audio_chunks(pcm)
        await self.send_tts_audio_data(pcm)

    async def on_response_done(self, status: str) -> None:
        request = self._request
        if request is None or request.closed:
            return
        if status != "completed":
            await self._fail_request(
                request,
                KalpaTTSError(
                    "response_cancelled",
                    "Kalpa ended the response before it completed",
                ),
            )
            return
        request.done += 1
        self._watch(request)
        await self._complete_if_done(request)

    async def on_response_error(self, error: KalpaTTSError) -> None:
        self.ten_env.log_error(
            f"vendor_error: {error.error_type}: {error.message}",
            category=LOG_CATEGORY_VENDOR,
        )
        request = self._request
        if request is not None and not request.closed:
            await self._fail_request(request, error)

    async def on_connection_failed(
        self, error: KalpaTTSError, will_retry: bool
    ) -> None:
        request = self._request
        active = request is not None and not request.closed
        if not will_retry:
            # The key, the account, the voice or the model has to change
            # before a permanent failure can go away. Connections that kept
            # failing for another reason are tried again at the next
            # request, so giving up on them is not fatal.
            module_error = self._module_error(error, fatal=error.permanent)
            if active:
                await self._end_request(request, ERROR, module_error)
            else:
                await self.send_tts_error(request_id="", error=module_error)
            return
        if not active:
            # An idle connection that drops is replaced without an error.
            return
        module_error = self._module_error(error, fatal=False)
        if (
            request.audio_started_at is not None
            # An `overloaded` refusal names the utterance, but nothing of it
            # was generated. Any other failure of an utterance would repeat.
            or (
                error.response_id is not None and error.error_type != OVERLOADED
            )
            or request.replays >= MAX_REPLAYS
        ):
            await self._end_request(request, ERROR, module_error)
            return
        # Nothing was heard yet: keep the request and send its text again
        # once the new session is ready.
        request.replays += 1
        request.sent = request.flushes_sent = request.done = 0
        request.send_started_at = request.first_audio_at = None
        request.carry = b""
        self._watch(request)
        await self.send_tts_error(
            request_id=request.request_id, error=module_error
        )

    # ── Ending a request ───────────────────────────────────────────

    async def cancel_tts(self) -> None:
        request = self._request
        if request is not None:
            # A new session, so text Kalpa has not spoken and audio not yet
            # read are gone before the next request.
            await self._end_request(request, INTERRUPTED, replace_client=True)
        elif self._ending is not None and not self._ending.done():
            # The request was already ending. Its last events go out before
            # the base class reports the flush as done.
            await asyncio.shield(self._ending)

    async def _complete_if_done(self, request: _Request) -> None:
        if (
            request.input_ended
            and not request.closed
            and request.sent == len(request.ops)
            and request.done >= request.flushes
        ):
            await self._end_request(request, REQUEST_END)

    async def _fail_request(
        self, request: _Request, error: KalpaTTSError
    ) -> None:
        """End a request that failed while its session is still open.

        Kalpa may still hold text or audio of it, so the session is replaced
        before the next request can use it.
        """
        await self._end_request(
            request,
            ERROR,
            self._module_error(error, fatal=False),
            replace_client=True,
        )

    def _watch(self, request: _Request) -> None:
        """Restart the deadline, or drop it when Kalpa owes no audio.

        Kalpa owes audio from the end of an utterance to its `responseDone`.
        Before that it may be waiting for the rest of a sentence.
        """
        if request.deadline is not None:
            request.deadline.cancel()
            request.deadline = None
        if not request.closed and request.done < request.flushes_sent:
            request.deadline = asyncio.get_running_loop().call_later(
                RESPONSE_TIMEOUT_S, self._on_response_timeout, request
            )

    def _on_response_timeout(self, request: _Request) -> None:
        if request.closed:
            return
        error = KalpaTTSError(
            "response_timeout",
            f"Kalpa sent nothing for {RESPONSE_TIMEOUT_S:g} seconds",
        )
        self.ten_env.log_error(
            f"vendor_error: {error.error_type}: {error.message}",
            category=LOG_CATEGORY_VENDOR,
        )
        # The request is not sent again: the listener has already waited.
        self._close_request(
            request, ERROR, self._module_error(error, fatal=False), True
        )

    async def _end_request(
        self,
        request: _Request,
        reason: TTSAudioEndReason,
        error: ModuleError | None = None,
        replace_client: bool = False,
    ) -> None:
        """End the request once. The events survive a cancelled caller."""
        await asyncio.shield(
            self._close_request(request, reason, error, replace_client)
        )

    def _close_request(
        self,
        request: _Request,
        reason: TTSAudioEndReason,
        error: ModuleError | None,
        replace_client: bool,
    ) -> asyncio.Task:
        """Close the request and return the task that emits its last events.

        Nothing here waits: once it returns, nothing else is emitted for
        the request, and a client that was replaced delivers nothing more.
        """
        if request.end_task is None:
            request.closed = True
            request.end_reason = reason
            self._watch(request)
            if self._request is request:
                self._request = None
            client = None
            if replace_client:
                client, self.client = self.client, None
                if client is not None:
                    client.stop()
            request.end_task = asyncio.create_task(
                self._emit_end(request, reason, error, client is not None)
            )
            self._ending = request.end_task
            if reason != REQUEST_END and not request.input_ended:
                # The rest of its input may still arrive. A request whose
                # text has ended is not remembered: an agent may speak
                # again under the same id.
                if len(self._ended) >= MAX_ENDED_REQUESTS:
                    self._ended.pop(next(iter(self._ended)))
                self._ended[request.request_id] = request
        return request.end_task

    async def _emit_end(
        self,
        request: _Request,
        reason: TTSAudioEndReason,
        error: ModuleError | None,
        client_stopped: bool,
    ) -> None:
        """Emit the last events of a request.

        Each step runs even if the one before it failed, so a failure to
        report cannot leave the extension without a session or the base
        class waiting for a request that is over.
        """
        request_id = request.request_id
        if self._stopped:
            return
        if client_stopped:
            message = "flush" if reason == INTERRUPTED else "request failed"
            await self._attempt(self.on_disconnected(code=0, message=message))
            if not self._stopped:
                # Started before the base class is told the request is
                # over, so the next request waits for this session.
                self._start_client()
        if error is not None:
            await self._attempt(
                self.send_tts_error(request_id=request_id, error=error)
            )
        await self._close_recorder(request_id)
        # A request that failed before any audio has no audio to end.
        started_at = request.audio_started_at
        audio_ended = reason != ERROR or started_at is not None
        if audio_ended:
            await self._attempt(
                self.send_tts_audio_end(
                    request_id=request_id,
                    request_event_interval_ms=(
                        int((time.monotonic() - started_at) * 1000)
                        if started_at is not None
                        else 0
                    ),
                    request_total_audio_duration_ms=int(
                        request.audio_bytes
                        * 1000
                        / (
                            self.synthesize_audio_sample_rate()
                            * self._sample_bytes()
                        )
                    ),
                    reason=reason,
                )
            )
        if audio_ended or request.counted:
            # Reported here, text that went to Kalpa for a request without
            # audio is not added to the usage of the next request.
            await self._attempt(self.send_usage_metrics(request_id))
        # After a flush the base class clears the request itself.
        if reason != INTERRUPTED:
            await self._attempt(self.finish_request(request_id, reason=reason))

    async def _attempt(self, step: Awaitable[None]) -> None:
        try:
            await step
        except Exception:
            self.ten_env.log_error(
                f"A step of ending a request failed: {traceback.format_exc()}"
            )

    async def _close_recorder(self, request_id: str) -> None:
        recorder = self._recorders.pop(request_id, None)
        if recorder is not None:
            await self._attempt(recorder.flush())

    def _sample_bytes(self) -> int:
        return (
            self.synthesize_audio_sample_width()
            * self.synthesize_audio_channels()
        )

    def _module_error(self, error: KalpaTTSError, fatal: bool) -> ModuleError:
        return ModuleError(
            message=error.message,
            module=ModuleType.TTS,
            code=(
                ModuleErrorCode.FATAL_ERROR
                if fatal
                else ModuleErrorCode.NON_FATAL_ERROR
            ),
            vendor_info=ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=error.error_type,
                message=error.message,
            ),
        )
