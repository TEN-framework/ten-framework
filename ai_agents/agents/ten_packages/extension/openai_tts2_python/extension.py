#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from datetime import datetime
import json
import os
import traceback

from ten_ai_base.helper import PCMWriter
from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from ten_ai_base.tts2 import RequestState
from ten_ai_base.const import LOG_CATEGORY_VENDOR, LOG_CATEGORY_KEY_POINT
from ten_runtime import AsyncTenEnv, Cmd, StatusCode
from ten_runtime.cmd_result import CmdResult
from uap_utils import TaskInfo

from .config import OpenAITTSConfig
from .openai_tts import OpenAITTSClient


CMD_IN_EVENT = "ten_event"
EVENTTYPE_START = "start"
CMD_PROPERTY_TASKINFO = "taskInfo"
CMD_PROPERTY_PAYLOAD = "payload"


class OpenAITTSExtension(AsyncTTS2HttpExtension):
    """
    OpenAI TTS Extension implementation.

    Provides text-to-speech synthesis using OpenAI's HTTP API.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: OpenAITTSConfig = None
        self.client: OpenAITTSClient = None
        self.task: TaskInfo | None = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create OpenAI TTS configuration from JSON string."""
        return OpenAITTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create OpenAI TTS client."""
        return OpenAITTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "openai"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config and self.config.params:
            sample_rate = self.config.params.get("sample_rate")
            if sample_rate is not None:
                try:
                    return int(sample_rate)
                except (TypeError, ValueError):
                    pass
        return 24000

    def _context_enabled(self) -> bool:
        return bool(
            self.config
            and self.config.params.get("enable_session_context", False)
        )

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd: {cmd_name}")
        if cmd_name == CMD_IN_EVENT:
            event_type, _ = cmd.get_property_string("type")
            if event_type == EVENTTYPE_START:
                buf, _ = cmd.get_property_buf(CMD_PROPERTY_PAYLOAD)
                event = json.loads(buf)
                ten_env.log_debug(f"event start {event}")
                if CMD_PROPERTY_TASKINFO in event:
                    self.task = TaskInfo.model_validate(
                        event[CMD_PROPERTY_TASKINFO]
                    )
                    ten_env.log_debug(f"task info: {self.task}")
                    if self.client is not None:
                        self.client.set_task(self.task)
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)
            ten_env.log_debug(f"on_cmd: {cmd_name} end")
            return

        await super().on_cmd(ten_env, cmd)
        ten_env.log_debug(f"on_cmd: {cmd_name} end")

    async def request_tts(self, t: TTSTextInput) -> None:
        """Handle TTS requests and pass context markers to the client."""
        try:
            self.ten_env.log_info(
                f"Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}",
            )
            if self.client is None:
                self.ten_env.log_debug(
                    "TTS client is not initialized, attempting to reinitialize..."
                )
                self.client = await self.create_client(
                    config=self.config,
                    ten_env=self.ten_env,
                )
                if self.task is not None:
                    self.client.set_task(self.task)
                self.ten_env.log_debug("TTS client reinitialized successfully.")

            self.ten_env.log_debug(
                f"current_request_id: {self.current_request_id}, new request_id: {t.request_id}, current_request_finished: {self.current_request_finished}"
            )

            is_first_chunk = t.request_id != self.current_request_id
            if is_first_chunk:
                self.ten_env.log_debug(
                    f"New TTS request with ID: {t.request_id}"
                )
                self.first_chunk = True
                self.sent_ts = datetime.now()
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)
                if self.config and self.config.dump:
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_debug(
                                f"Cleaned up old PCMWriter for request_id: {old_rid}"
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"{self.vendor()}_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_debug(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )
            elif self.current_request_finished:
                self.ten_env.log_error(
                    f"Received a message for a finished request_id '{t.request_id}' with text_input_end=False."
                )
                return

            if t.text_input_end:
                self.ten_env.log_debug(
                    f"finish session for request ID: {t.request_id}"
                )
                self.current_request_finished = True

            self.metrics_add_output_characters(len(t.text))

            self.ten_env.log_debug(
                f"send_text_to_tts_server:  {t.text} of request_id: {t.request_id}",
            )
            data = self.client.get(
                t.text,
                t.request_id,
                is_first_chunk=is_first_chunk,
                is_end=t.text_input_end,
                request_seq_id=self._resolve_request_seq_id(t),
                context=(
                    await self._build_context_payload(t)
                    if is_first_chunk and self._context_enabled()
                    else None
                ),
            )

            chunk_count = 0

            async for audio_chunk, event_status in data:
                if event_status == TTS2HttpResponseEventType.RESPONSE:
                    if audio_chunk is not None and len(audio_chunk) > 0:
                        chunk_count += 1
                        self.total_audio_bytes += len(audio_chunk)
                        duration_ms = self._calculate_audio_duration_ms()
                        self.ten_env.log_debug(
                            f"receive_audio:  duration: {duration_ms} of request id: {self.current_request_id}",
                            category=LOG_CATEGORY_VENDOR,
                        )

                        if self.first_chunk:
                            self.request_ts = datetime.now()
                            if self.sent_ts:
                                await self.send_tts_audio_start(
                                    request_id=self.current_request_id,
                                )
                                ttfb = int(
                                    (
                                        datetime.now() - self.sent_ts
                                    ).total_seconds()
                                    * 1000
                                )
                                extra_metadata = (
                                    self.client.get_extra_metadata()
                                )
                                await self.send_tts_ttfb_metrics(
                                    request_id=self.current_request_id,
                                    ttfb_ms=ttfb,
                                    extra_metadata=extra_metadata,
                                )
                                self.ten_env.log_debug(
                                    f"Sent TTS audio start and TTFB metrics: {ttfb}ms"
                                )
                            self.first_chunk = False

                        if (
                            self.config
                            and self.config.dump
                            and self.current_request_id
                            and self.current_request_id in self.recorder_map
                        ):
                            await self.recorder_map[
                                self.current_request_id
                            ].write(audio_chunk)

                        self.metrics_add_recv_audio_chunks(audio_chunk)
                        await self.send_tts_audio_data(audio_chunk)
                    else:
                        self.ten_env.log_debug(
                            "Received empty payload for TTS response"
                        )
                        if self.request_ts and t.text_input_end:
                            await self._send_audio_end_and_finish(
                                request_id=self.current_request_id,
                                reason=TTSAudioEndReason.REQUEST_END,
                                log_message=f"Sent TTS audio end event, interval: {self._calculate_request_event_interval_ms()}ms, duration: {self._calculate_audio_duration_ms()}ms",
                            )

                elif event_status == TTS2HttpResponseEventType.END:
                    self.ten_env.log_debug(
                        "Received TTS_END event from TTS"
                    )
                    if self.request_ts and t.text_input_end:
                        await self._send_audio_end_and_finish(
                            request_id=self.current_request_id,
                            reason=TTSAudioEndReason.REQUEST_END,
                            log_message=f"Sent TTS audio end event, interval: {self._calculate_request_event_interval_ms()}ms, duration: {self._calculate_audio_duration_ms()}ms",
                        )
                    break

                elif event_status == TTS2HttpResponseEventType.FLUSH:
                    self.ten_env.log_debug(
                        "Received TTS_FLUSH event from TTS"
                    )
                    if self.request_ts:
                        await self._send_audio_end_and_finish(
                            request_id=self.current_request_id,
                            reason=TTSAudioEndReason.INTERRUPTED,
                        )
                    break

                elif event_status == TTS2HttpResponseEventType.INVALID_KEY_ERROR:
                    error_msg = (
                        audio_chunk.decode("utf-8")
                        if audio_chunk
                        else "Unknown API key error"
                    )
                    request_id = self.current_request_id or t.request_id
                    await self._handle_error_with_text_input_end(
                        request_id=request_id,
                        error=ModuleError(
                            message=error_msg,
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        ),
                        text_input_end=t.text_input_end,
                    )
                    return

                elif event_status == TTS2HttpResponseEventType.ERROR:
                    error_msg = (
                        audio_chunk.decode("utf-8")
                        if audio_chunk
                        else "Unknown client error"
                    )
                    request_id = self.current_request_id or t.request_id
                    await self._handle_error_with_text_input_end(
                        request_id=request_id,
                        error=ModuleError(
                            message=error_msg,
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.NON_FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        ),
                        text_input_end=t.text_input_end,
                    )
                    return

            self.ten_env.log_debug(
                f"TTS processing completed, total chunks: {chunk_count}"
            )

            if self.request_ts and t.text_input_end and self.current_request_id:
                if self.current_request_id in self.request_states:
                    current_state = self.request_states[self.current_request_id]
                    if current_state == RequestState.FINALIZING:
                        self.ten_env.log_info(
                            f"Stream ended without END event for request {self.current_request_id}, sending audio_end",
                            category=LOG_CATEGORY_KEY_POINT,
                        )
                        await self._send_audio_end_and_finish(
                            request_id=self.current_request_id,
                            reason=TTSAudioEndReason.REQUEST_END,
                        )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            request_id = self.current_request_id or t.request_id
            await self._handle_error_with_text_input_end(
                request_id=request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
                text_input_end=t.text_input_end,
            )

    async def _build_context_payload(self, t: TTSTextInput) -> dict | None:
        """Read session context from metadata and pass it through unchanged."""
        if not self._context_enabled():
            return None
        if not isinstance(t.metadata, dict):
            return None

        session_context = t.metadata.get("session_context")
        if not isinstance(session_context, str):
            return None

        self.ten_env.log_debug(
            f"Resolved session_context from metadata for request_id={t.request_id}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        return {
            "context_text": session_context,
        }

    def _resolve_request_seq_id(self, t: TTSTextInput) -> int:
        """Resolve request_seq_id from TTSTextInput metadata."""
        if isinstance(t.metadata, dict):
            turn_seq_id = t.metadata.get("turn_seq_id")
            if isinstance(turn_seq_id, int):
                return turn_seq_id
        return 0
