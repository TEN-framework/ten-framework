import asyncio
import copy
import json
import os
from datetime import datetime
from typing import Any, Dict, List, Optional

from typing_extensions import override

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeDiscard,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.struct import ASRWord, TTSTextInput
from ten_ai_base.utils import redact_json, redact_url
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Data,
)

from .config import AssemblyAIASRConfig
from ten_ai_base.const import DATA_IN_ASR_FINALIZE

from .const import (
    AGENT_CONTEXT_MAX_CHARS,
    DATA_IN_TTS_TEXT_INPUT,
    DUMP_FILE_NAME,
    FATAL_CLOSE_CODES,
    FATAL_HTTP_STATUSES,
    NORMAL_CLOSE_CODES,
)
from .reconnect_manager import ReconnectManager
from .recognition import (
    AssemblyAIConnectionError,
    AssemblyAIWSRecognition,
    AssemblyAIWSRecognitionCallback,
)


class _FinalizeContext:
    """One in-flight asr_finalize handshake."""

    __slots__ = ("finalize_id", "metadata", "started_ms", "timeout_task")

    def __init__(
        self,
        finalize_id: Optional[str],
        metadata: Dict[str, Any],
        started_ms: int,
    ):
        self.finalize_id = finalize_id
        self.metadata = metadata
        self.started_ms = started_ms
        self.timeout_task: Optional[asyncio.Task] = None


class _EpochCallback(AssemblyAIWSRecognitionCallback):
    """Forwards one client's callbacks while that client is still current.

    A replaced client can still emit close or error callbacks; those must not
    touch the state of the connection that superseded it.
    """

    def __init__(self, extension: "AssemblyAIASRExtension", epoch: int):
        self._extension = extension
        self._epoch = epoch

    def _is_current(self, event: str) -> bool:
        if self._epoch == self._extension.connection_epoch:
            return True
        self._extension.ten_env.log_debug(
            f"ignoring {event} from stale connection epoch {self._epoch}"
        )
        return False

    async def on_open(self, session_id: str, configuration: Dict[str, Any]):
        if self._is_current("on_open"):
            await self._extension.on_open(session_id, configuration)

    async def on_result(self, message_data: Dict[str, Any]):
        if self._is_current("on_result"):
            await self._extension.on_result(message_data)

    async def on_event(self, message_data: Dict[str, Any]):
        if self._is_current("on_event"):
            await self._extension.on_event(message_data)

    async def on_error(self, error_msg: str, error_code: Optional[str] = None):
        if self._is_current("on_error"):
            await self._extension.on_error(error_msg, error_code)

    async def on_close(self, code: int, reason: str):
        if self._is_current("on_close"):
            await self._extension.on_close(code, reason)


class AssemblyAIASRExtension(
    AsyncASRBaseExtension, AssemblyAIWSRecognitionCallback
):
    """AssemblyAI Streaming (v3) ASR extension.

    Targets the Universal-3 Pro family (``universal-3-6-pro`` by default) and
    still supports the Universal Streaming models. The agent's spoken replies
    can be routed in as ``tts_text_input`` to feed ``agent_context``.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.recognition: Optional[AssemblyAIWSRecognition] = None
        self.config: Optional[AssemblyAIASRConfig] = None
        self.audio_dumper: Optional[Dumper] = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.reconnect_manager: Optional[ReconnectManager] = None
        # Model confirmed by the server in the `Begin` message.
        self.session_model: Optional[str] = None

        # Lifecycle latches and single-owner background work.
        self._init_failed = False
        self._fatal_latched = False
        self._connection_epoch = 0
        self._swapping = False
        self._swap_lock = asyncio.Lock()
        self._reconnect_task: Optional[asyncio.Task] = None
        self._connect_started_at: Optional[float] = None
        # Pending finalize handshakes, oldest first. Each entry owns its
        # finalize_id, session metadata and timeout task so overlapping
        # requests each get exactly one asr_finalize_end.
        self._pending_finalizes: List[_FinalizeContext] = []
        self._next_finalize_metadata: Dict[str, Any] = {}

        # agent_context bookkeeping (text of the agent reply in flight).
        self._agent_context_request_id: Optional[str] = None
        self._agent_context_parts: List[str] = []
        self._pending_agent_context: Optional[str] = None
        self._last_agent_context: Optional[str] = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        try:
            await self._cancel_background_tasks()
            if self.audio_dumper:
                await self.audio_dumper.stop()
                self.audio_dumper = None
            self.reconnect_manager = None
            self.config = None
        except Exception as e:
            ten_env.log_error(f"Error during deinit: {e}")

    @override
    def vendor(self) -> str:
        return "assemblyai"

    @property
    def connection_epoch(self) -> int:
        """Identity of the current vendor connection attempt."""
        return self._connection_epoch

    def _callback_for_epoch(
        self, epoch: int
    ) -> AssemblyAIWSRecognitionCallback:
        return _EpochCallback(self, epoch)

    @override
    def vendor_metadata(self) -> Dict[str, Any]:
        if self.config is None:
            return {}
        fields = {
            "key": self.config.api_key,
            # The base class masks JSON keys, not query values inside a
            # URL string, so mask any signed query here.
            "url": redact_url(self.config.ws_url),
            "model": self.config.speech_model,
            "mode": self.config.mode,
        }
        return {k: v for k, v in fields.items() if v}

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            config = AssemblyAIASRConfig.model_validate_json(config_json)
            config.update(config.params)
            config.validate_config()
            self.config = config

            ten_env.log_info(
                f"config: {config.to_redacted_dict()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if config.dump:
                dump_file_path = os.path.join(config.dump_path, DUMP_FILE_NAME)
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()

        except Exception as e:
            # One fatal error; later connection attempts are refused so a
            # fallback empty configuration is never used.
            self._init_failed = True
            self._fatal_latched = True
            if self.config is None:
                self.config = AssemblyAIASRConfig()
            message = f"Invalid AssemblyAI ASR config: {e}"
            ten_env.log_error(message)
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=message,
                ),
            )

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    @override
    async def start_connection(self) -> None:
        assert self.config is not None

        if self._init_failed or self._fatal_latched:
            message = "AssemblyAI ASR connection refused: fatal error latched"
            self.ten_env.log_warn(message)
            await self.on_disconnected(
                code=ModuleErrorCode.FATAL_ERROR.value, message=message
            )
            return

        if not self.config.api_key or not self.config.api_key.strip():
            error = ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=(
                    "AssemblyAI API key is required but not provided or is "
                    "empty"
                ),
            )
            self._fatal_latched = True
            self.ten_env.log_error(error.message)
            await self.send_asr_error(error)
            await self.on_disconnected(code=error.code, message=error.message)
            return

        self.ten_env.log_info("Starting AssemblyAI ASR connection")

        # Replace the client atomically; `is_connected()` is False meanwhile so
        # the base class does not hand audio to a client being torn down.
        async with self._swap_lock:
            self._swapping = True
            try:
                await self._close_recognition()

                dropped = self.config.dropped_connection_params()
                if dropped:
                    self.ten_env.log_warn(
                        f"params ignored for {self.config.speech_model}: "
                        f"{dropped}"
                    )

                connection_params = self.config.to_connection_params()
                self.ten_env.log_info(
                    "AssemblyAI ASR connection params: "
                    f"{redact_json(connection_params)}",
                    category=LOG_CATEGORY_KEY_POINT,
                )

                self._connection_epoch += 1
                recognition = AssemblyAIWSRecognition(
                    api_key=self.config.api_key,
                    ws_url=self.config.ws_url,
                    audio_timeline=self.audio_timeline,
                    ten_env=self.ten_env,
                    config=connection_params,
                    callback=self._callback_for_epoch(self._connection_epoch),
                )
                self.recognition = recognition
                self._connect_started_at = asyncio.get_running_loop().time()
            finally:
                self._swapping = False

        try:
            await recognition.start()
        except AssemblyAIConnectionError as e:
            await self._handle_connect_failure(
                recognition,
                message=str(e),
                vendor_code=e.code,
                fatal=e.code in FATAL_HTTP_STATUSES,
            )
            return
        except Exception as e:
            await self._handle_connect_failure(
                recognition,
                message=f"Failed to start AssemblyAI ASR connection: {e}",
                vendor_code=None,
                fatal=False,
            )
            return

        if self.stopped or self.recognition is not recognition:
            # Stopped or replaced while the handshake was in flight: this
            # session has no owner, so release it instead of letting it run.
            self.ten_env.log_info(
                "AssemblyAI ASR connection superseded during handshake; closing"
            )
            await recognition.close()
            if self.recognition is recognition:
                self.recognition = None
            return

        self.ten_env.log_info("AssemblyAI ASR connection started")

    async def _handle_connect_failure(
        self,
        recognition: AssemblyAIWSRecognition,
        message: str,
        vendor_code: Optional[str],
        fatal: bool,
    ) -> None:
        if self.recognition is not recognition:
            # This client was superseded (or stopped) while handshaking.
            # Its failure is history: release it and leave the live client
            # and the reported connection state alone.
            self.ten_env.log_info(
                "stale handshake failed after replacement; ignoring: "
                f"{message}"
            )
            await self._close_client(recognition)
            return

        self.ten_env.log_error(
            f"vendor_error: code: {vendor_code}, reason: {message}",
            category=LOG_CATEGORY_VENDOR,
        )
        vendor_info = None
        if vendor_code:
            vendor_info = ModuleErrorVendorInfo(
                vendor=self.vendor(), code=vendor_code, message=message
            )
        error = ModuleError(
            module=ModuleType.ASR,
            code=(
                ModuleErrorCode.FATAL_ERROR.value
                if fatal
                else ModuleErrorCode.NON_FATAL_ERROR.value
            ),
            message=message,
        )
        await self.send_asr_error(error, vendor_info)

        await self._close_recognition()
        if fatal:
            self._fatal_latched = True
        await self.on_disconnected(
            code=error.code, message=message, vendor_info=vendor_info
        )

        if fatal:
            self.ten_env.log_error(
                "AssemblyAI ASR connection failed fatally; not reconnecting"
            )
            return
        await self._handle_reconnect()

    async def _close_recognition(self) -> None:
        """Release the current client (idempotent, never raises)."""
        recognition, self.recognition = self.recognition, None
        await self._close_client(recognition)

    async def _close_client(
        self, recognition: Optional[AssemblyAIWSRecognition]
    ) -> None:
        if recognition is None:
            return
        try:
            await recognition.close()
        except Exception as e:
            self.ten_env.log_warn(f"Error closing AssemblyAI client: {e}")

    @override
    async def stop_connection(self) -> None:
        self._swapping = True
        try:
            await self._cancel_background_tasks()
            # Callers may still be waiting on finalize handshakes.
            await self._complete_all_finalizes("stop")
            await self._close_recognition()
            self.ten_env.log_info("AssemblyAI ASR connection stopped")
        except Exception as e:
            self.ten_env.log_error(
                f"Error stopping AssemblyAI ASR connection: {e}"
            )
        finally:
            self._swapping = False

    @override
    def is_connected(self) -> bool:
        return (
            not self._swapping
            and self.recognition is not None
            and self.recognition.is_connected()
        )

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        # AssemblyAI rejects audio sent faster than real time (close code
        # 3007), so replaying a backlog after a reconnect is not an option.
        return ASRBufferConfigModeDiscard()

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    # ------------------------------------------------------------------
    # Reconnect: one owner, one background task at a time
    # ------------------------------------------------------------------

    async def _handle_reconnect(self) -> None:
        """Schedule a reconnect attempt unless one is pending or forbidden."""
        if self.stopped or self._fatal_latched:
            self.ten_env.log_debug("reconnect skipped: stopped or fatal")
            return
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        pending = self._reconnect_task
        if (
            pending
            and not pending.done()
            and pending is not asyncio.current_task()
        ):
            self.ten_env.log_debug("reconnect already pending")
            return

        if not self.reconnect_manager.can_retry():
            self._fatal_latched = True
            self._reconnect_task = None
            self.ten_env.log_error("No more reconnection attempts allowed")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message="No more reconnection attempts allowed",
                )
            )
            return

        self._reconnect_task = asyncio.create_task(self._run_reconnect())

    async def _run_reconnect(self) -> None:
        assert self.reconnect_manager is not None
        try:
            await self.reconnect_manager.handle_reconnect(
                connection_func=self.start_connection,
                error_handler=self.send_asr_error,
            )
        except Exception as e:  # CancelledError is not an Exception
            self.ten_env.log_error(f"Reconnect attempt crashed: {e}")

    async def _cancel_background_tasks(self) -> None:
        current = asyncio.current_task()
        reconnect = self._reconnect_task
        if reconnect and not reconnect.done() and reconnect is not current:
            reconnect.cancel()
            try:
                await reconnect
            except asyncio.CancelledError:
                pass
        if reconnect is not current:
            self._reconnect_task = None

    # ------------------------------------------------------------------
    # Audio in
    # ------------------------------------------------------------------

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        assert self.config is not None

        # Snapshot the client once; the base class may only hand audio to
        # the connection that was checked, never to a replacement.
        recognition = self.recognition
        if recognition is None or not self.is_connected():
            self.ten_env.log_debug("send_audio: no live AssemblyAI session")
            return False

        buf = None
        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)
        finally:
            if buf is not None:
                frame.unlock_buf(buf)

        try:
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)
        except Exception as e:
            self.ten_env.log_warn(f"audio dump failed: {e}")

        if self.recognition is not recognition or not self.is_connected():
            # Replaced or disconnected while the dump was awaited.
            self.ten_env.log_debug("send_audio: session changed; frame dropped")
            return False

        try:
            await recognition.send_audio_frame(audio_data)
            return True
        except Exception as e:
            self.ten_env.log_error(
                f"Error sending audio to AssemblyAI ASR: {e}"
            )
            return False

    # ------------------------------------------------------------------
    # Finalize: exactly one asr_finalize_end per request, always bounded
    # ------------------------------------------------------------------

    @property
    def last_finalize_timestamp(self) -> int:
        """Start time (ms) of the oldest pending finalize, 0 when idle."""
        return (
            self._pending_finalizes[0].started_ms
            if self._pending_finalizes
            else 0
        )

    @override
    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        # The base class stored this request's finalize_id and session
        # metadata just before calling us; capture them so the response
        # echoes this request even if another finalize arrives meanwhile.
        request_metadata, self._next_finalize_metadata = (
            self._next_finalize_metadata,
            {},
        )
        context = _FinalizeContext(
            finalize_id=self.finalize_id,
            metadata=(
                request_metadata
                or (copy.deepcopy(self.metadata) if self.metadata else {})
            ),
            started_ms=int(datetime.now().timestamp() * 1000),
        )
        self._pending_finalizes.append(context)
        self.ten_env.log_debug(
            f"AssemblyAI ASR finalize start id={context.finalize_id} "
            f"at {context.started_ms}"
        )

        if not (self.recognition and self.is_connected()):
            self.ten_env.log_warn(
                "finalize while disconnected; completing immediately",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self._complete_finalize(context)
            return

        await self.recognition.force_endpoint()
        context.timeout_task = asyncio.create_task(
            self._finalize_timeout(context, self.config.finalize_timeout_ms)
        )

    async def _finalize_timeout(
        self, context: "_FinalizeContext", timeout_ms: int
    ) -> None:
        await asyncio.sleep(timeout_ms / 1000)
        context.timeout_task = None
        if context in self._pending_finalizes:
            self.ten_env.log_warn(
                f"no final turn within {timeout_ms}ms of finalize "
                f"{context.finalize_id}; completing finalize",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self._complete_finalize(context)

    async def _finalize_end(self) -> None:
        """Complete the oldest pending finalize (a final turn arrived)."""
        if self._pending_finalizes:
            await self._complete_finalize(self._pending_finalizes[0])

    async def _complete_all_finalizes(self, reason: str) -> None:
        while self._pending_finalizes:
            self.ten_env.log_warn(
                f"completing finalize {self._pending_finalizes[0].finalize_id} "
                f"on {reason}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self._complete_finalize(self._pending_finalizes[0])

    async def _complete_finalize(self, context: "_FinalizeContext") -> None:
        """Emit exactly one asr_finalize_end for ``context``."""
        if context not in self._pending_finalizes:
            return
        self._pending_finalizes.remove(context)

        task, context.timeout_task = context.timeout_task, None
        if task and not task.done() and task is not asyncio.current_task():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

        latency = int(datetime.now().timestamp() * 1000) - context.started_ms
        self.ten_env.log_debug(
            f"AssemblyAI ASR finalize end id={context.finalize_id}, "
            f"latency: {latency}ms"
        )
        # send_asr_finalize_end() reads the base-class fields; restore this
        # request's identity so overlapping requests do not swap responses.
        self.finalize_id = context.finalize_id
        saved_metadata = self.metadata
        if context.metadata:
            self.metadata = context.metadata
        try:
            await self.send_asr_finalize_end()
        finally:
            self.metadata = saved_metadata

    # ------------------------------------------------------------------
    # Data in: agent replies -> agent_context
    # ------------------------------------------------------------------

    @override
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() == DATA_IN_ASR_FINALIZE:
            # The base class only forwards finalize_id; the request's own
            # metadata (session_id) must be captured here so the response
            # echoes it even when the latest audio frame belongs elsewhere.
            self._next_finalize_metadata = self._read_metadata(data)
        await super().on_data(ten_env, data)
        if data.get_name() == DATA_IN_TTS_TEXT_INPUT:
            await self._on_tts_text_input(data)

    @staticmethod
    def _read_metadata(data: Data) -> Dict[str, Any]:
        raw, err = data.get_property_to_json("metadata")
        if err or not raw:
            return {}
        try:
            value = json.loads(raw)
        except ValueError:
            return {}
        return value if isinstance(value, dict) else {}

    async def _on_tts_text_input(self, data: Data) -> None:
        try:
            raw, err = data.get_property_to_json(None)
            if err:
                self.ten_env.log_warn(f"tts_text_input: invalid payload: {err}")
                return
            text_input = TTSTextInput.model_validate_json(raw)
        except Exception as e:
            self.ten_env.log_warn(f"tts_text_input: cannot parse: {e}")
            return

        if (
            self._agent_context_request_id is not None
            and text_input.request_id != self._agent_context_request_id
        ):
            # A new reply started before the previous one was marked complete.
            await self._flush_agent_context()

        self._agent_context_request_id = text_input.request_id
        self._agent_context_parts.append(text_input.text)

        if text_input.text_input_end:
            await self._flush_agent_context()

    async def _flush_agent_context(self) -> None:
        text = "".join(self._agent_context_parts).strip()
        self._agent_context_parts = []
        self._agent_context_request_id = None
        if not text:
            return
        await self._send_agent_context(text[-AGENT_CONTEXT_MAX_CHARS:])

    async def _send_agent_context(self, text: str) -> None:
        assert self.config is not None
        if not self.config.is_pro_model:
            self.ten_env.log_debug(
                "agent_context is only supported by the Universal-3 Pro "
                f"family; ignoring for {self.config.speech_model}"
            )
            return

        self._last_agent_context = text
        if self.recognition and self.is_connected():
            await self.recognition.send_update_configuration(
                {"agent_context": text}
            )
        else:
            self._pending_agent_context = text

    # ------------------------------------------------------------------
    # Vendor callbacks
    # ------------------------------------------------------------------

    @override
    async def on_open(
        self, session_id: str, configuration: Dict[str, Any]
    ) -> None:
        if self.stopped:
            self.ten_env.log_info("session opened after stop; ignoring")
            return

        self.ten_env.log_info(
            f"vendor_status_changed: on_open session_id={session_id} "
            f"configuration={configuration}",
            category=LOG_CATEGORY_VENDOR,
        )
        if isinstance(configuration, dict):
            self.session_model = configuration.get("model") or None

        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()

        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()

        await self.on_connected()

        if self._connect_started_at is not None:
            delay_ms = int(
                (asyncio.get_running_loop().time() - self._connect_started_at)
                * 1000
            )
            self._connect_started_at = None
            await self.send_connect_delay_metrics(delay_ms)

        # Seed the fresh session with the latest agent reply so the model
        # knows what question the user is answering (also after reconnects).
        context = self._pending_agent_context or self._last_agent_context
        self._pending_agent_context = None
        if context:
            await self._send_agent_context(context)

    @override
    async def on_result(self, message_data: Dict[str, Any]) -> None:
        assert self.config is not None
        try:
            transcript = message_data.get("transcript") or ""
            end_of_turn = bool(message_data.get("end_of_turn", False))
            turn_is_formatted = bool(message_data.get("turn_is_formatted"))
            is_final_turn = end_of_turn and (
                turn_is_formatted or not self.config.format_turns
            )

            if not transcript:
                # An empty end-of-turn is not a user turn, but it does
                # acknowledge a pending finalize (silence after ForceEndpoint).
                if is_final_turn and self._pending_finalizes:
                    self.ten_env.log_debug(
                        "empty final turn acknowledges finalize"
                    )
                    await self._finalize_end()
                return

            result = self._build_asr_result(message_data)
            self.ten_env.log_debug(
                f"AssemblyAI ASR result: {result.text}, final: {result.final}, "
                f"start_ms: {result.start_ms}, duration_ms: {result.duration_ms}"
            )
            await self.send_asr_result(result)
            if result.final:
                await self._finalize_end()
        except Exception as e:
            self.ten_env.log_error(
                f"Error processing AssemblyAI ASR result: {e}"
            )

    def _to_user_time_ms(self, vendor_ms: Any) -> int:
        """Map a vendor timestamp to the user's audio timeline."""
        return int(
            self.audio_timeline.get_audio_duration_before_time(
                int(vendor_ms or 0)
            )
            + self.sent_user_audio_duration_ms_before_last_reset
        )

    def _build_asr_result(self, turn: Dict[str, Any]) -> ASRResult:
        assert self.config is not None

        raw_words: List[Dict[str, Any]] = turn.get("words") or []
        words: List[ASRWord] = []
        for raw in raw_words:
            start = int(raw.get("start", 0) or 0)
            end = int(raw.get("end", start) or start)
            words.append(
                ASRWord(
                    word=str(raw.get("text", "")),
                    start_ms=self._to_user_time_ms(start),
                    duration_ms=max(0, end - start),
                    stable=bool(raw.get("word_is_final", False)),
                )
            )

        if raw_words:
            first_start = int(raw_words[0].get("start", 0) or 0)
            last = raw_words[-1]
            last_end = int(last.get("end", last.get("start", 0)) or 0)
            start_ms = self._to_user_time_ms(first_start)
            duration_ms = max(0, last_end - first_start)
        else:
            start_ms = self._to_user_time_ms(0)
            duration_ms = 0

        end_of_turn = bool(turn.get("end_of_turn", False))
        turn_is_formatted = bool(turn.get("turn_is_formatted", False))
        final = end_of_turn and (
            turn_is_formatted or not self.config.format_turns
        )

        language_code = turn.get("language_code")
        language = (
            self.config.language_for_code(language_code)
            if language_code
            else self.config.normalized_language
        )

        asr_info: Dict[str, Any] = {
            "end_of_turn": end_of_turn,
            "turn_is_formatted": turn_is_formatted,
        }
        for key in (
            "turn_order",
            "end_of_turn_confidence",
            "language_code",
            "language_confidence",
            "speaker_label",
        ):
            if turn.get(key) is not None:
                asr_info[key] = turn[key]

        return ASRResult(
            text=turn.get("transcript", ""),
            final=final,
            start_ms=start_ms,
            duration_ms=duration_ms,
            language=language,
            words=words,
            metadata=self._build_metadata_with_asr_info(asr_info),
        )

    def _build_metadata_with_asr_info(
        self, additional_fields: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Metadata per protocol: session_id at root, the rest in asr_info."""
        assert self.config is not None
        base_metadata = (
            copy.deepcopy(self.metadata) if self.metadata is not None else {}
        )
        session_id = base_metadata.pop("session_id", None)

        asr_info: Dict[str, Any] = copy.deepcopy(base_metadata)
        asr_info["vendor"] = self.vendor()
        asr_info["model"] = self.session_model or self.config.speech_model
        if additional_fields:
            asr_info.update(additional_fields)

        metadata: Dict[str, Any] = {}
        if session_id is not None:
            metadata["session_id"] = session_id
        metadata["asr_info"] = asr_info
        return metadata

    @override
    async def on_event(self, message_data: Dict[str, Any]) -> None:
        self.ten_env.log_info(
            f"vendor_status_changed: on_event message_data: {message_data}",
            category=LOG_CATEGORY_VENDOR,
        )

    @override
    async def on_error(
        self, error_msg: str, error_code: Optional[str] = None
    ) -> None:
        self.ten_env.log_error(
            f"vendor_error: code: {error_code}, reason: {error_msg}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=error_msg,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=error_code or "unknown",
                message=error_msg,
            ),
        )

    @override
    async def on_close(self, code: int, reason: str) -> None:
        self.ten_env.log_info(
            f"vendor_status_changed: on_close code={code} reason='{reason}'",
            category=LOG_CATEGORY_VENDOR,
        )

        fatal = code in FATAL_CLOSE_CODES
        vendor_info: Optional[ModuleErrorVendorInfo] = None
        module_code = 0
        message = reason or "closed"

        if code not in NORMAL_CLOSE_CODES:
            message = reason or f"connection closed with code {code}"
            vendor_info = ModuleErrorVendorInfo(
                vendor=self.vendor(), code=str(code), message=message
            )
            error = ModuleError(
                module=ModuleType.ASR,
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=message,
            )
            await self.send_asr_error(error, vendor_info)
            module_code = error.code

        if fatal:
            self._fatal_latched = True

        await self.on_disconnected(
            code=module_code, message=message, vendor_info=vendor_info
        )

        if fatal:
            self.ten_env.log_error(
                f"AssemblyAI closed the session with code {code}; "
                "not reconnecting"
            )
            return
        if not self.stopped:
            await self._handle_reconnect()
