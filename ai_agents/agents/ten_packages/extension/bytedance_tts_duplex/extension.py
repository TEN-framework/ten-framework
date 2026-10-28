#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os
import time
import traceback
from datetime import datetime
from typing import Any

from ten_ai_base import utils
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_TRANSCRIPTS,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    ModuleVendorException,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput, TTSTextResult, TTSWord
from ten_ai_base.transcription import TurnStatus
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import AsyncTenEnv

from .bytedance_tts import (
    BytedanceV3Client,
    EVENT_SessionFinished,
    EVENT_TTSError,
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
    EVENT_TTSSentenceStart,
    EVENT_TTSSubtitle,
)
from .config import BytedanceTTSDuplexConfig


def _mask_metadata_secret(value: Any) -> Any:
    return utils.encrypt(value) if value else value


def _initial_subtitle_turn_seq_id(metadata: dict) -> int | None:
    turn_seq_id = metadata.get("turn_seq_id")
    if isinstance(turn_seq_id, int) and not isinstance(turn_seq_id, bool):
        return turn_seq_id
    return None


class BytedanceTTSDuplexExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BytedanceTTSDuplexConfig | None = None
        self.client: BytedanceV3Client | None = None
        self.current_request_id: str | None = None
        self.current_vendor_session_id: str | None = None
        self.stop_event: asyncio.Event | None = None
        self.msg_polling_task: asyncio.Task | None = None
        self.recorder: PCMWriter | None = None
        self.first_text_sent_at: float | None = None
        self.first_audio_received_at: float | None = None
        self.request_start_ts: datetime | None = None
        self.request_ttfb: int | None = None
        # Timestamp when the first audio frame is actually sent to downstream.
        # Used to compute request_event_interval based on "played" time rather than
        # wall time since request submission.
        self.first_audio_sent_ts: datetime | None = None
        self.response_msgs = asyncio.Queue[tuple[int, bytes | dict | None]]()
        self.recorder_map: dict[str, PCMWriter] = {}
        self.last_completed_request_id: str | None = None
        self.last_completed_has_reset_synthesizer = True
        self.is_reconnecting = False
        self.current_metadata: dict | None = None
        self.next_subtitle_turn_seq_id: int | None = None
        self.audio_timestamp_base_ms: int = 0
        self.subtitle_timestamp_origin_ms: int | None = None
        # Timestamp of the first audio frame of the current request. Unlike
        # current_sentence_start_ms, it is never moved by sentence start/end
        # events, so it is a stable anchor for subtitle timestamps that are
        # cumulative over the whole session (seed-tts 2.0, EVENT_TTSSubtitle).
        self.request_audio_start_ms: int | None = None
        self.current_sentence_start_ms: int = 0
        self.accumulated_audio_duration_ms: int = 0
        self.total_audio_bytes: int = 0  # Track actual received audio bytes
        self.is_first_message_of_request: bool = False
        self.input_end_received: bool = False
        self.current_request_end_reason = TTSAudioEndReason.REQUEST_END
        self.pending_config: BytedanceTTSDuplexConfig | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = BytedanceTTSDuplexConfig.model_validate_json(config_json)

                # extract audio_params and additions from config
                self.config.update_params()

            self.ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.client = BytedanceV3Client(
                self.config,
                self.ten_env,
                self.vendor(),
                self.response_msgs,
                self._on_error,
                self._handle_usage_characters,
                self._on_fatal_failure,
                self.on_connecting,
                self.on_connected,
                self.on_disconnected,
                self._on_text_send,
                self._on_audio_receive,
            )
            self.msg_polling_task = asyncio.create_task(self._loop())
        except Exception as e:
            ten_env.log_error(f"on_start failed: {traceback.format_exc()}")
            await self.send_tts_error(
                self.current_request_id or "",
                ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(),
                ),
            )

    def _on_text_send(self, session_id: str, sent_at: float) -> None:
        """Anchor TTFB to the first text sent for the active vendor session."""
        if (
            session_id == self.current_vendor_session_id
            and self.first_text_sent_at is None
        ):
            self.first_text_sent_at = sent_at

    def _on_audio_receive(self, session_id: str, received_at: float) -> None:
        if (
            session_id == self.current_vendor_session_id
            and self.first_audio_received_at is None
        ):
            self.first_audio_received_at = received_at

    async def _on_error(self, error: ModuleError) -> None:
        """Handle all errors from TTS client (both recoverable and non-recoverable)."""
        await self.send_tts_error(self.current_request_id or "", error)

    async def _on_fatal_failure(self, error: ModuleError) -> None:
        """Handle fatal connection failure (retries exhausted or unrecoverable error)."""
        self.ten_env.log_error(f"Fatal connection failure: {error}")

        # Only finish if input_end has been received
        if self.input_end_received and self.current_request_id:
            self.ten_env.log_error(
                f"Fatal failure after input_end, finishing request {self.current_request_id}.",
            )
            await self.finish_request(
                self.current_request_id,
                reason=TTSAudioEndReason.ERROR,
                error=error,
            )
            return

        await self.send_tts_error(self.current_request_id or "", error)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # close the client connection
        if self.client:
            await self.client.close()

        if self.msg_polling_task:
            self.msg_polling_task.cancel()

        # close all PCMWriter
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_info(f"Flushed PCMWriter for request_id: {request_id}")
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}",
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def update_configs(self, configs: dict[str, Any]) -> None:
        """
        Called when update_configs command is received.
        Accumulate updates in pending_config by merging.
        """
        try:
            self.ten_env.log_info(
                f"tts_update_configs: {configs}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config:
                # Use a temporary config to merge and validate before updating pending_config
                base_config = (
                    self.pending_config if self.pending_config else self.config
                )
                new_config = base_config.model_copy(deep=True)
                new_config.merge_with_dict(configs)

                # If validation passes, update pending_config
                self.pending_config = new_config
        except Exception as e:
            self.ten_env.log_error(f"Failed to update configs: {e}")
            raise

    def vendor(self) -> str:
        return "bytedance_duplex"

    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}

        explicit_headers = self.config.headers or {}
        authorization = explicit_headers.get(
            "Authorization",
            "",
        ) or explicit_headers.get("authorization", "")

        metadata = {
            "name": self.vendor(),
            "authorization": authorization,
            "url": self.config.api_url,
            "model": self.config.model,
            "app_id": self.config.app_id,
            "token": self.config.token,
            "api_key": self.config.api_key,
            "x_api_key": _mask_metadata_secret(explicit_headers.get("X-Api-Key", "")),
            "x_api_access_key": _mask_metadata_secret(
                explicit_headers.get("X-Api-Access-Key", ""),
            ),
            "speaker": self.config.speaker,
            "resource_id": self.config.resource_id,
        }
        return {key: value for key, value in metadata.items() if value}

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def _calculate_audio_duration_ms(self) -> int:
        """Calculate total audio duration in milliseconds based on received bytes."""
        if self.config is None:
            return 0
        bytes_per_sample = self.synthesize_audio_sample_width()  # Usually 2 for 16-bit
        channels = self.synthesize_audio_channels()  # Usually 1 for mono
        sample_rate = self.synthesize_audio_sample_rate()

        if self.total_audio_bytes == 0:
            return 0

        duration_sec = self.total_audio_bytes / (
            sample_rate * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    async def _loop(self) -> None:
        while True:
            try:
                event, data = await self.response_msgs.get()

                if event == EVENT_TTSResponse:
                    if data is not None and isinstance(data, bytes):
                        audio_data = data
                        # Billing: received audio chunks
                        self.metrics_add_recv_audio_chunks(audio_data)

                        # Track total audio bytes for duration calculation
                        self.total_audio_bytes += len(audio_data)

                        self.ten_env.log_info(
                            f"KEYPOINT Received audio data for request ID: {self.current_request_id}, audio_data_len: {len(audio_data)}",
                        )

                        # Calculate chunk duration
                        chunk_duration = self.calculate_audio_duration(
                            len(audio_data),
                            self.synthesize_audio_sample_rate(),
                            self.synthesize_audio_channels(),
                            self.synthesize_audio_sample_width(),
                        )
                        self.ten_env.log_debug(
                            f"receive_audio: duration: {chunk_duration} of request id: {self.current_request_id}",
                            category=LOG_CATEGORY_VENDOR,
                        )

                        if (
                            self.config.dump
                            and self.current_request_id
                            and self.current_request_id in self.recorder_map
                        ):
                            await self.recorder_map[self.current_request_id].write(
                                audio_data,
                            )
                        if (
                            self.first_text_sent_at is not None
                            and self.first_audio_received_at is not None
                            and self.request_ttfb is None
                        ):
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTSAudioStart for request ID: {self.current_request_id}",
                            )
                            self.first_audio_sent_ts = datetime.now()
                            await self.send_tts_audio_start(
                                request_id=self.current_request_id or "",
                            )
                            elapsed_time = int(
                                (self.first_audio_received_at - self.first_text_sent_at)
                                * 1000,
                            )
                            await self.send_tts_ttfb_metrics(
                                request_id=self.current_request_id or "",
                                ttfb_ms=elapsed_time,
                                extra_metadata={
                                    "model": (self.config.model if self.config else ""),
                                    "speaker": (
                                        self.config.speaker if self.config else ""
                                    ),
                                },
                            )
                            self.request_ttfb = elapsed_time
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTFB metrics for request ID: {self.current_request_id}, elapsed time: {elapsed_time}ms",
                            )

                        if self.audio_timestamp_base_ms == 0:
                            self.audio_timestamp_base_ms = int(
                                datetime.now().timestamp() * 1000,
                            )
                            self.current_sentence_start_ms = (
                                self.audio_timestamp_base_ms
                            )
                            self.request_audio_start_ms = self.audio_timestamp_base_ms

                        audio_timestamp = (
                            self.current_sentence_start_ms
                            + self.accumulated_audio_duration_ms
                        )

                        self.accumulated_audio_duration_ms += chunk_duration
                        await self.send_tts_audio_data(audio_data, audio_timestamp)
                    else:
                        self.ten_env.log_error(
                            "Received empty payload for TTS response",
                        )
                elif event == EVENT_TTSSentenceStart:
                    self._handle_sentence_start()
                elif event == EVENT_TTSSentenceEnd:
                    await self._handle_sentence_end(data)
                elif event == EVENT_TTSSubtitle:
                    await self._handle_subtitle(data)
                elif event == EVENT_SessionFinished:
                    request_id = self.current_request_id
                    if request_id is None or not isinstance(data, dict):
                        continue

                    event_session_id = data.get("session_id")
                    if event_session_id != self.current_vendor_session_id:
                        self.ten_env.log_warn(
                            f"Ignoring SessionFinished for stale vendor session {event_session_id}",
                        )
                        continue

                    if self.current_request_end_reason == TTSAudioEndReason.ERROR:
                        self.ten_env.log_info(
                            f"Ignoring SessionFinished after error for request ID: {request_id}",
                        )
                        if self.stop_event:
                            self.stop_event.set()
                        continue

                    self.ten_env.log_info(
                        f"KEYPOINT Session finished for request ID: {request_id}",
                    )
                    await self._handle_transcription(None, True)
                    if self.request_start_ts is not None:
                        request_event_interval = (
                            int(
                                (
                                    datetime.now() - self.first_audio_sent_ts
                                ).total_seconds()
                                * 1000,
                            )
                            if self.first_audio_sent_ts is not None
                            else 0
                        )
                        duration_ms = self._calculate_audio_duration_ms()
                        await self.send_tts_audio_end(
                            request_id=request_id,
                            request_event_interval_ms=request_event_interval,
                            request_total_audio_duration_ms=duration_ms,
                        )

                        self.ten_env.log_info(
                            f"KEYPOINT request time stamped for request ID: {self.current_request_id}, request_event_interval: {request_event_interval}ms, total_audio_duration: {duration_ms}ms",
                        )

                    await self._flush_dump(request_id)

                    await self.send_usage_metrics(request_id)

                    # 重置状态为下一个请求做准备
                    self.current_request_id = None
                    self.current_vendor_session_id = None
                    self.request_start_ts = None
                    self.request_ttfb = None
                    self.first_audio_sent_ts = None
                    self.total_audio_bytes = 0

                    if self.stop_event:
                        self.stop_event.set()
                        self.stop_event = None
                elif event == EVENT_TTSError:
                    # A protocol error ends the current vendor session, but this
                    # loop must not call finish_request(). It emits the terminal
                    # outputs and wakes request_tts(), which remains the single
                    # owner of base request completion.
                    if not isinstance(data, dict):
                        continue

                    request_id = self.current_request_id
                    if request_id is None:
                        continue

                    if data.get("session_id") != self.current_vendor_session_id:
                        self.ten_env.log_warn(
                            f"Ignoring error for stale vendor session {data.get('session_id')}",
                        )
                        continue

                    if self.current_request_end_reason == TTSAudioEndReason.ERROR:
                        continue

                    self.current_request_end_reason = TTSAudioEndReason.ERROR

                    error = ModuleError(
                        message=f"recv vendor error, message: {data['message']}, code: {data['code']}",
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(data["code"]),
                            message=str(data["message"]),
                        ),
                    )
                    await self.send_tts_error(request_id, error)

                    request_event_interval = (
                        int(
                            (datetime.now() - self.first_audio_sent_ts).total_seconds()
                            * 1000,
                        )
                        if self.first_audio_sent_ts is not None
                        else 0
                    )
                    duration_ms = self._calculate_audio_duration_ms()
                    await self.send_tts_audio_end(
                        request_id=request_id,
                        request_event_interval_ms=request_event_interval,
                        request_total_audio_duration_ms=duration_ms,
                        reason=TTSAudioEndReason.ERROR,
                    )
                    await self.send_usage_metrics(request_id)

                    if self.stop_event:
                        # Let request_tts() reset the connection and call
                        # finish_request() exactly once.
                        self.stop_event.set()

            except Exception:
                self.ten_env.log_error(f"Error in _loop: {traceback.format_exc()}")
                if self.stop_event:
                    self.stop_event.set()
                    self.stop_event = None

                # If loop crashes, we must finish the current request to avoid deadlock
                # We assume loop crash is a fatal error for the current session
                if self.current_request_id:
                    await self.finish_request(
                        self.current_request_id,
                        reason=TTSAudioEndReason.ERROR,
                        error=ModuleError(
                            message="Error in audio processing loop",
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR.value,
                            vendor_info=ModuleErrorVendorInfo(),
                        ),
                    )

    def _handle_sentence_start(self) -> None:
        self.ten_env.log_info(
            f"Received sentence start event for request ID: {self.current_request_id}",
        )
        self.current_sentence_start_ms = self.audio_timestamp_base_ms
        self.accumulated_audio_duration_ms = 0

    async def _handle_sentence_end(self, data: Any) -> None:
        self.ten_env.log_info(
            f"Received sentence end event for request ID: {self.current_request_id}",
        )
        self.audio_timestamp_base_ms = (
            self.current_sentence_start_ms + self.accumulated_audio_duration_ms
        )
        if isinstance(data, dict):
            await self._handle_transcription(data, False)
        self.current_sentence_start_ms = self.audio_timestamp_base_ms
        self.accumulated_audio_duration_ms = 0

    async def _handle_subtitle(self, data: Any) -> None:
        if isinstance(data, dict):
            await self._handle_transcription(
                data,
                False,
                use_session_timestamps=True,
            )

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS: text_length={len(t.text)}, "
                f"text_input_end: {t.text_input_end} "
                f"request ID: {t.request_id}",
            )
            self.ten_env.log_debug(
                f"KEYPOINT Requesting TTS for text: {t.text}, "
                f"text_input_end: {t.text_input_end} "
                f"request ID: {t.request_id}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )

            if self.client is None:
                self.ten_env.log_error("Client is not initialized")
                return

            # check if the request_id has already been completed
            if (
                self.last_completed_request_id
                and t.request_id == self.last_completed_request_id
            ):
                error_msg = f"Request ID {t.request_id} has already been completed (last completed: {self.last_completed_request_id})"
                self.ten_env.log_error(error_msg)
                return
            if t.request_id != self.current_request_id:
                if self.pending_config and self.client:
                    self.ten_env.log_info(
                        f"Applying pending configs for new request {t.request_id}",
                    )
                    # Apply the pre-merged and pre-validated pending config
                    self.config = self.pending_config

                    # Sync the new config to the manager
                    self.client.update_config(self.config)

                    # Trigger client reconnection with new config
                    await self.client.cancel()
                    self.ten_env.log_info(
                        "Client reconnection triggered with new configuration.",
                    )
                    self.pending_config = None

                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}",
                )
                if not self.last_completed_has_reset_synthesizer:
                    await self.client.reset_synthesizer()
                self.last_completed_has_reset_synthesizer = False

                self.current_request_id = t.request_id
                self.current_vendor_session_id = self.client.session_id
                self.first_text_sent_at = None
                self.first_audio_received_at = None
                self.request_start_ts = datetime.now()
                self.request_ttfb = None
                self.first_audio_sent_ts = None
                self.current_metadata = t.metadata
                self.next_subtitle_turn_seq_id = _initial_subtitle_turn_seq_id(
                    self.current_metadata,
                )
                self.audio_timestamp_base_ms = 0
                self.subtitle_timestamp_origin_ms = None
                self.request_audio_start_ms = None
                self.total_audio_bytes = 0  # Reset for new request
                self.is_first_message_of_request = True
                self.input_end_received = False
                self.current_request_end_reason = TTSAudioEndReason.REQUEST_END

                if self.config.dump:
                    old_request_ids = [
                        rid for rid in self.recorder_map if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_info(
                                f"Cleaned up old PCMWriter for request_id: {old_rid}",
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}",
                            )

                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"bytedance_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(dump_file_path)
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}",
                        )

                # Early finish: only when this is the first message of the request and no content
                if (
                    t.text.strip() == ""
                    and t.text_input_end
                    and self.is_first_message_of_request
                ):
                    self.ten_env.log_info(
                        f"KEYPOINT empty-first-message with text_input_end=True; finishing immediately for request ID: {t.request_id}",
                    )
                    # mark as completed to prevent future duplicates
                    self.last_completed_request_id = t.request_id

                    # compute request interval if started
                    request_event_interval = 0
                    if self.request_start_ts is not None:
                        request_event_interval = (
                            int(
                                (
                                    datetime.now() - self.first_audio_sent_ts
                                ).total_seconds()
                                * 1000,
                            )
                            if self.first_audio_sent_ts is not None
                            else 0
                        )

                    # duration is 0 for empty audio
                    await self.send_tts_audio_end(
                        request_id=self.current_request_id or "",
                        request_event_interval_ms=request_event_interval,
                        request_total_audio_duration_ms=0,
                    )

                    await self.send_usage_metrics(self.current_request_id)

                    # proactive finish connection and reset synthesizer to keep state consistent
                    if self.client is not None:
                        try:
                            await self.client.finish_connection()
                        except Exception as e:
                            self.ten_env.log_error(
                                f"finish_connection error on empty-end: {e}",
                            )
                        await self.client.reset_synthesizer()

                    # Notify base class request finished
                    if self.current_request_id:
                        await self.finish_request(self.current_request_id)

                    # reset state
                    self.current_request_id = None
                    self.request_start_ts = None
                    self.request_ttfb = None
                    self.total_audio_bytes = 0
                    self.last_completed_has_reset_synthesizer = True
                    return

            request_stop_event: asyncio.Event | None = None
            if t.text_input_end:
                request_stop_event = asyncio.Event()
                self.stop_event = request_stop_event

            if t.text.strip() != "":
                # Billing: output characters
                self.metrics_add_output_characters(len(t.text))
                await self.send_tts_request_metrics(
                    request_id=t.request_id,
                    request_time_ms=int(time.time() * 1000),
                    request_text=t.text,
                )
                await self.client.send_text(t.text)
                # after handling first non-empty message, mark flag false
                self.is_first_message_of_request = False

            # Track input_end status
            if t.text_input_end:
                self.input_end_received = True

                self.ten_env.log_info(
                    f"KEYPOINT received text_input_end for request ID: {t.request_id}",
                )

                # update the latest completed request_id
                self.last_completed_request_id = t.request_id
                self.ten_env.log_info(
                    f"Updated last completed request_id to: {t.request_id}",
                )

                await self.client.finish_session()

                assert request_stop_event is not None
                if self.current_request_end_reason == TTSAudioEndReason.ERROR:
                    request_stop_event.set()
                await request_stop_event.wait()

                # session finished, connection will be re-established for next request
                if not self.last_completed_has_reset_synthesizer:
                    await self.client.finish_connection()
                    await self.client.reset_synthesizer()
                    self.last_completed_has_reset_synthesizer = True

                end_reason = self.current_request_end_reason
                if self.current_request_id == t.request_id:
                    self.current_request_id = None
                    self.current_vendor_session_id = None
                    self.request_start_ts = None
                    self.request_ttfb = None
                    self.first_audio_sent_ts = None
                    self.total_audio_bytes = 0
                self.stop_event = None
                self.current_request_end_reason = TTSAudioEndReason.REQUEST_END

                # Both SessionFinished and EVENT_TTSError wake this coroutine;
                # request completion is deliberately centralized here.
                await self.finish_request(t.request_id, reason=end_reason)

        except ModuleVendorException as e:
            self.ten_env.log_error(
                f"ModuleVendorException in request_tts: {traceback.format_exc()}",
            )
            if t.text_input_end:
                await self.finish_request(
                    self.current_request_id,
                    reason=TTSAudioEndReason.ERROR,
                    error=ModuleError(
                        message=f"recv vendor error, message: {e.error.message}, code: {e.error.code}",
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        vendor_info=e.error,
                    ),
                )
        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}",
            )
            if t.text_input_end:
                await self.finish_request(
                    self.current_request_id,
                    reason=TTSAudioEndReason.ERROR,
                    error=ModuleError(
                        message=str(e),
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        vendor_info=ModuleErrorVendorInfo(),
                    ),
                )

    async def _handle_transcription(
        self,
        transcription_data: dict | None,
        is_final: bool,
        use_session_timestamps: bool = False,
    ):
        try:
            if not self.config.enable_words:
                return

            if transcription_data is None and not is_final:
                return

            self.ten_env.log_info(
                f"KEYPOINT _handle_transcription: current_sentence_start_ms: {self.current_sentence_start_ms}, accumulated_audio_duration_ms: {self.accumulated_audio_duration_ms}",
            )

            if is_final:
                self.ten_env.log_info(
                    f"KEYPOINT received final transcription for request ID: {self.current_request_id}",
                )
                transcription = TTSTextResult(
                    request_id=self.current_request_id or "",
                    text="",
                    start_ms=self.current_sentence_start_ms,
                    duration_ms=0,
                    words=[],
                    text_result_end=True,  # Bytedance sends all words at the end
                    metadata=self._transcription_metadata(TurnStatus.END),
                )

                self.ten_env.log_info(
                    f"KEYPOINT transcription: "
                    f"request_id={transcription.request_id}, "
                    f"text_length={len(transcription.text)}",
                )
                self.ten_env.log_debug(
                    f"KEYPOINT transcription details: {transcription}",
                    category=LOG_CATEGORY_TRANSCRIPTS,
                )
                await self.send_tts_text_result(transcription)
                self.ten_env.log_info(
                    f"send tts_text_result: request_id={self.current_request_id}, "
                    f"text_length={len(transcription.text)}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
                self.ten_env.log_debug(
                    f"send tts_text_result details: {transcription}",
                    category=LOG_CATEGORY_TRANSCRIPTS,
                )
                return

            words_json = transcription_data.get("words", [])
            text = transcription_data.get("text", "")

            words = []
            first_word_start_sec = -1
            last_word_end_sec = -1

            for word_json in words_json:
                start_time_sec = word_json.get("startTime", 0.0)
                end_time_sec = word_json.get("endTime", 0.0)
                word_text = word_json.get("word", "")

                if first_word_start_sec < 0:
                    first_word_start_sec = start_time_sec
                last_word_end_sec = max(last_word_end_sec, end_time_sec)

                words.append(
                    TTSWord(
                        word=word_text,
                        # The start_ms calculation will be adjusted later
                        start_ms=int(start_time_sec * 1000),
                        duration_ms=int((end_time_sec - start_time_sec) * 1000),
                    ),
                )

            if not words:
                return

            self._align_subtitle_words(
                words,
                first_word_start_sec,
                use_session_timestamps=use_session_timestamps,
            )

            # Log the generated words
            self.ten_env.log_debug(
                f"generate_words: request_id={self.current_request_id}, "
                f"word_count={len(words)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            self.ten_env.log_debug(
                f"generate_words details: {words} "
                f"of request id: {self.current_request_id}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )

            first_word = words[0]
            last_word = words[-1]
            duration_ms = (
                last_word.start_ms + last_word.duration_ms - first_word.start_ms
            )

            transcription = TTSTextResult(
                request_id=self.current_request_id or "",
                text=text,
                start_ms=first_word.start_ms,
                duration_ms=duration_ms,
                words=words,
                text_result_end=is_final,  # Bytedance sends all words at the end
                metadata=self._transcription_metadata(TurnStatus.IN_PROGRESS),
            )

            self.ten_env.log_info(
                f"KEYPOINT transcription: "
                f"request_id={transcription.request_id}, "
                f"text_length={len(transcription.text)}",
            )
            self.ten_env.log_debug(
                f"KEYPOINT transcription details: {transcription}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )
            await self.send_tts_text_result(transcription)
            if self.next_subtitle_turn_seq_id is not None:
                self.next_subtitle_turn_seq_id += 1
            self.ten_env.log_info(
                f"send tts_text_result: request_id={self.current_request_id}, "
                f"text_length={len(transcription.text)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            self.ten_env.log_debug(
                f"send tts_text_result details: {transcription}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )

        except Exception:
            self.ten_env.log_error(
                f"Error in _handle_transcription: {traceback.format_exc()}",
            )

    def _align_subtitle_words(
        self,
        words: list[TTSWord],
        first_word_start_sec: float,
        use_session_timestamps: bool = False,
    ) -> None:
        if use_session_timestamps:
            if self.subtitle_timestamp_origin_ms is None:
                self.subtitle_timestamp_origin_ms = int(first_word_start_sec * 1000)
            origin_ms = self.subtitle_timestamp_origin_ms
            # The offset relative to the latched origin already covers all audio
            # elapsed since the start of the request, so it must be added to a
            # fixed anchor. Using current_sentence_start_ms here would double
            # count that elapsed audio every time a sentence event moves it.
            anchor_ms = (
                self.request_audio_start_ms
                if self.request_audio_start_ms is not None
                else self.current_sentence_start_ms
            )
            for word in words:
                word.start_ms = anchor_ms + (word.start_ms - origin_ms)
        else:
            # Sentence-end word times are cumulative within the request.
            # Preserve their first-word offset by anchoring on the request's
            # first audio frame instead of normalizing each sentence to zero.
            anchor_ms = (
                self.request_audio_start_ms
                if self.request_audio_start_ms is not None
                else self.current_sentence_start_ms
            )
            for word in words:
                word.start_ms += anchor_ms

    def _transcription_metadata(self, turn_status: TurnStatus) -> dict:
        metadata = dict(self.current_metadata or {})
        if self.next_subtitle_turn_seq_id is not None:
            metadata["turn_seq_id"] = self.next_subtitle_turn_seq_id
        metadata["turn_status"] = int(turn_status)
        return metadata

    def _handle_usage_characters(self, char_count: int):
        """处理计费字符数回调"""
        try:
            # 更新计量统计 - 输入字符数
            self.metrics_add_input_characters(char_count)
            self.ten_env.log_info(f"Updated usage characters: {char_count}")
        except Exception as e:
            self.ten_env.log_error(f"Failed to handle usage characters: {e}")

    async def _flush_dump(self, request_id: str | None) -> None:
        """Persist the dump file of one request if dumping is enabled."""
        if not (self.config and self.config.dump and request_id):
            return
        recorder = self.recorder_map.get(request_id)
        if recorder is None:
            return
        try:
            await recorder.flush()
        except Exception as e:
            self.ten_env.log_error(
                f"Error flushing PCMWriter for request_id {request_id}: {e}",
            )

    async def cancel_tts(self) -> None:
        """Override cancel_tts to handle bytedance duplex-specific cancellation logic"""
        try:
            self.last_completed_request_id = self.current_request_id
            # Cancel current connection (maintain original flush disconnect behavior)
            if self.client:
                await self.client.cancel()
                self.last_completed_has_reset_synthesizer = True

            await self._flush_dump(self.current_request_id)

            # If there's a waiting stop_event, set it to release request_tts waiting
            if self.stop_event:
                self.stop_event.set()
                self.stop_event = None

            self.ten_env.log_info("Executing cancel_tts for bytedance_duplex")

            request_event_interval = 0
            if self.request_start_ts:
                request_event_interval = (
                    int(
                        (datetime.now() - self.first_audio_sent_ts).total_seconds()
                        * 1000,
                    )
                    if self.first_audio_sent_ts is not None
                    else 0
                )
                duration_ms = self._calculate_audio_duration_ms()
                await self.send_tts_audio_end(
                    request_id=self.current_request_id or "",
                    request_event_interval_ms=request_event_interval,
                    request_total_audio_duration_ms=duration_ms,
                    reason=TTSAudioEndReason.INTERRUPTED,
                )
                await self.send_usage_metrics(self.current_request_id)
                self.request_start_ts = None
                self.request_ttfb = None
                self.first_audio_sent_ts = None
                self.total_audio_bytes = 0

                self.ten_env.log_info(
                    f"Sent tts_audio_end with INTERRUPTED reason for request_id: {self.current_request_id}",
                )
        except Exception as e:
            self.ten_env.log_error(f"Error in cancel_tts: {e}")

    def calculate_audio_duration(
        self,
        bytes_length: int,
        sample_rate: int,
        channels: int = 1,
        sample_width: int = 2,
    ) -> int:
        """
        Calculate audio duration in milliseconds.

        Parameters:
        - bytes_length: Length of the audio data in bytes
        - sample_rate: Sample rate in Hz (e.g., 16000)
        - channels: Number of audio channels (default: 1 for mono)
        - sample_width: Number of bytes per sample (default: 2 for 16-bit PCM)

        Returns:
        - Duration in milliseconds (rounded down to nearest int)
        """
        bytes_per_second = sample_rate * channels * sample_width
        duration_seconds = bytes_length / bytes_per_second
        return int(duration_seconds * 1000)
