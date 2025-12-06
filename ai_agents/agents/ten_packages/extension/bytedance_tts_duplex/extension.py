#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os
import traceback
from datetime import datetime
from typing import Dict, Tuple, Union

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
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
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import AsyncTenEnv

from .bytedance_tts import (
    BytedanceV3Client,
    EVENT_SessionFinished,
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
    EVENT_TTSSentenceStart,
)
from .config import BytedanceTTSDuplexConfig


class BytedanceTTSDuplexExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BytedanceTTSDuplexConfig | None = None
        self.client: BytedanceV3Client | None = None
        self.current_request_id: str | None = None
        self.stop_event: asyncio.Event | None = None
        self.msg_polling_task: asyncio.Task | None = None
        self.recorder: PCMWriter | None = None
        self.request_start_ts: datetime | None = None
        self.request_ttfb: int | None = None
        self.response_msgs = asyncio.Queue[
            Tuple[int, Union[bytes, dict, None]]
        ]()
        self.recorder_map: dict[str, PCMWriter] = {}
        self.last_completed_request_id: str | None = None
        self.last_completed_has_reset_synthesizer = True
        self.is_reconnecting = False
        self.current_metadata: Dict | None = None
        self.audio_timestamp_base_ms: int = 0
        self.current_sentence_start_ms: int = 0
        self.accumulated_audio_duration_ms: int = 0
        self.total_audio_bytes: int = 0  # Track actual received audio bytes
        self.is_first_message_of_request: bool = False
        self.input_end_received: bool = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = BytedanceTTSDuplexConfig.model_validate_json(
                    config_json
                )

                # extract audio_params and additions from config
                self.config.update_params()
                self.config.validate_params()

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

    def _on_error(self, error: ModuleError):
        """Handle all errors from TTS client (both recoverable and non-recoverable)."""
        # Create a task to handle the async operation
        asyncio.create_task(
            self.send_tts_error(self.current_request_id or "", error)
        )

    async def _on_fatal_failure(self, error: ModuleError):
        """Handle fatal connection failure (retries exhausted or unrecoverable error)."""
        self.ten_env.log_error(f"Fatal connection failure: {error}")

        # Only finish if input_end has been received
        if self.input_end_received and self.current_request_id:
            self.ten_env.log_error(
                f"Fatal failure after input_end, finishing request {self.current_request_id}."
            )
            await self.finish_request(
                self.current_request_id,
                reason=TTSAudioEndReason.ERROR,
                error=error,
            )

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
                ten_env.log_info(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "bytedance_duplex"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def _calculate_audio_duration_ms(self) -> int:
        """Calculate total audio duration in milliseconds based on received bytes."""
        if self.config is None:
            return 0
        bytes_per_sample = (
            self.synthesize_audio_sample_width()
        )  # Usually 2 for 16-bit
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
                            f"KEYPOINT Received audio data for request ID: {self.current_request_id}, audio_data_len: {len(audio_data)}"
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
                            await self.recorder_map[
                                self.current_request_id
                            ].write(audio_data)
                        if (
                            self.request_start_ts is not None
                            and self.request_ttfb is None
                        ):
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTSAudioStart for request ID: {self.current_request_id}"
                            )
                            await self.send_tts_audio_start(
                                request_id=self.current_request_id or ""
                            )
                            elapsed_time = int(
                                (
                                    datetime.now() - self.request_start_ts
                                ).total_seconds()
                                * 1000
                            )
                            await self.send_tts_ttfb_metrics(
                                request_id=self.current_request_id or "",
                                ttfb_ms=elapsed_time,
                                extra_metadata={
                                    "model": (
                                        self.config.model if self.config else ""
                                    ),
                                    "speaker": (
                                        self.config.speaker
                                        if self.config
                                        else ""
                                    ),
                                },
                            )
                            self.request_ttfb = elapsed_time
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTFB metrics for request ID: {self.current_request_id}, elapsed time: {elapsed_time}ms"
                            )

                        if self.audio_timestamp_base_ms == 0:
                            self.audio_timestamp_base_ms = int(
                                datetime.now().timestamp() * 1000
                            )
                            self.current_sentence_start_ms = (
                                self.audio_timestamp_base_ms
                            )

                        audio_timestamp = (
                            self.current_sentence_start_ms
                            + self.accumulated_audio_duration_ms
                        )

                        self.accumulated_audio_duration_ms += chunk_duration
                        await self.send_tts_audio_data(
                            audio_data, audio_timestamp
                        )
                    else:
                        self.ten_env.log_error(
                            "Received empty payload for TTS response"
                        )
                elif event == EVENT_TTSSentenceStart:
                    self.ten_env.log_info(
                        f"Received sentence start event for request ID: {self.current_request_id}"
                    )
                    self.current_sentence_start_ms = (
                        self.audio_timestamp_base_ms
                    )
                    self.accumulated_audio_duration_ms = 0
                elif event == EVENT_TTSSentenceEnd:
                    self.ten_env.log_info(
                        f"Received sentence end event for request ID: {self.current_request_id}"
                    )
                    self.audio_timestamp_base_ms = (
                        self.current_sentence_start_ms
                        + self.accumulated_audio_duration_ms
                    )
                    if data is not None and isinstance(data, dict):
                        await self._handle_transcription(data, False)
                elif event == EVENT_SessionFinished:
                    self.ten_env.log_info(
                        f"KEYPOINT Session finished for request ID: {self.current_request_id}"
                    )
                    await self._handle_transcription(None, True)
                    if self.request_start_ts is not None:
                        request_event_interval = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
                        )
                        duration_ms = self._calculate_audio_duration_ms()
                        await self.send_tts_audio_end(
                            request_id=self.current_request_id or "",
                            request_event_interval_ms=request_event_interval,
                            request_total_audio_duration_ms=duration_ms,
                        )

                        self.ten_env.log_info(
                            f"KEYPOINT request time stamped for request ID: {self.current_request_id}, request_event_interval: {request_event_interval}ms, total_audio_duration: {duration_ms}ms"
                        )

                    await self.send_usage_metrics(self.current_request_id)

                    # 重置状态为下一个请求做准备
                    self.current_request_id = None
                    self.request_start_ts = None
                    self.request_ttfb = None
                    self.total_audio_bytes = 0

                    if self.stop_event:
                        self.stop_event.set()
                        self.stop_event = None

            except Exception:
                self.ten_env.log_error(
                    f"Error in _loop: {traceback.format_exc()}"
                )
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

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
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
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )
                if not self.last_completed_has_reset_synthesizer:
                    self.client.reset_synthesizer()
                self.last_completed_has_reset_synthesizer = False

                self.current_request_id = t.request_id
                self.request_start_ts = datetime.now()
                self.request_ttfb = None
                self.current_metadata = t.metadata
                self.audio_timestamp_base_ms = 0
                self.total_audio_bytes = 0  # Reset for new request
                self.is_first_message_of_request = True
                self.input_end_received = False

                if self.config.dump:
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_info(
                                f"Cleaned up old PCMWriter for request_id: {old_rid}"
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"bytendance_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )

                # Early finish: only when this is the first message of the request and no content
                if (
                    t.text.strip() == ""
                    and t.text_input_end
                    and self.is_first_message_of_request
                ):
                    self.ten_env.log_info(
                        f"KEYPOINT empty-first-message with text_input_end=True; finishing immediately for request ID: {t.request_id}"
                    )
                    # mark as completed to prevent future duplicates
                    self.last_completed_request_id = t.request_id

                    # compute request interval if started
                    request_event_interval = 0
                    if self.request_start_ts is not None:
                        request_event_interval = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
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
                                f"finish_connection error on empty-end: {e}"
                            )
                        self.client.reset_synthesizer()

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

            if t.text.strip() != "":
                # Billing: output characters
                self.metrics_add_output_characters(len(t.text))
                await self.client.send_text(t.text)
                # after handling first non-empty message, mark flag false
                self.is_first_message_of_request = False

            # Track input_end status
            if t.text_input_end:
                self.input_end_received = True

                self.ten_env.log_info(
                    f"KEYPOINT received text_input_end for request ID: {t.request_id}"
                )

                # update the latest completed request_id
                self.last_completed_request_id = t.request_id
                self.ten_env.log_info(
                    f"Updated last completed request_id to: {t.request_id}"
                )

                await self.client.finish_session()

                self.stop_event = asyncio.Event()
                await self.stop_event.wait()

                # session finished, connection will be re-established for next request
                if not self.last_completed_has_reset_synthesizer:
                    await self.client.finish_connection()
                    self.client.reset_synthesizer()
                    self.last_completed_has_reset_synthesizer = True

                await self.finish_request(t.request_id)

        except ModuleVendorException as e:
            self.ten_env.log_error(
                f"ModuleVendorException in request_tts: {traceback.format_exc()}. text: {t.text}"
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
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
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
        self, transcription_data: dict | None, is_final: bool
    ):
        try:
            if not self.config.enable_words:
                return

            if transcription_data is None and not is_final:
                return

            self.ten_env.log_info(
                f"KEYPOINT _handle_transcription: current_sentence_start_ms: {self.current_sentence_start_ms}, accumulated_audio_duration_ms: {self.accumulated_audio_duration_ms}"
            )

            if is_final:
                self.ten_env.log_info(
                    f"KEYPOINT received final transcription for request ID: {self.current_request_id}"
                )
                transcription = TTSTextResult(
                    request_id=self.current_request_id or "",
                    text="",
                    start_ms=self.current_sentence_start_ms,
                    duration_ms=0,
                    words=[],
                    text_result_end=True,  # Bytedance sends all words at the end
                    metadata=self.current_metadata or {},
                )

                self.ten_env.log_info(
                    f"KEYPOINT transcription: {transcription}"
                )
                await self.send_tts_text_result(transcription)
                self.ten_env.log_info(
                    f"send tts_text_result: {transcription} of request id: {self.current_request_id}",
                    category=LOG_CATEGORY_KEY_POINT,
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
                    )
                )

            if not words:
                return

            # Adjust word timestamps to be absolute
            for word in words:
                word.start_ms = self.current_sentence_start_ms + (
                    word.start_ms - int(first_word_start_sec * 1000)
                )

            # Log the generated words
            self.ten_env.log_debug(
                f"generate_words: generate_words: {words} of request id: {self.current_request_id}",
                category=LOG_CATEGORY_KEY_POINT,
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
                metadata=self.current_metadata or {},
            )

            self.ten_env.log_info(f"KEYPOINT transcription: {transcription}")
            await self.send_tts_text_result(transcription)
            self.ten_env.log_info(
                f"send tts_text_result: {transcription} of request id: {self.current_request_id}",
                category=LOG_CATEGORY_KEY_POINT,
            )

        except Exception:
            self.ten_env.log_error(
                f"Error in _handle_transcription: {traceback.format_exc()}"
            )

    def _handle_usage_characters(self, char_count: int):
        """处理计费字符数回调"""
        try:
            # 更新计量统计 - 输入字符数
            self.metrics_add_input_characters(char_count)
            self.ten_env.log_info(f"Updated usage characters: {char_count}")
        except Exception as e:
            self.ten_env.log_error(f"Failed to handle usage characters: {e}")

    async def cancel_tts(self) -> None:
        """Override cancel_tts to handle bytedance duplex-specific cancellation logic"""
        try:
            self.last_completed_request_id = self.current_request_id
            # Cancel current connection (maintain original flush disconnect behavior)
            if self.client:
                self.client.cancel()
                self.last_completed_has_reset_synthesizer = True

            # If there's a waiting stop_event, set it to release request_tts waiting
            if self.stop_event:
                self.stop_event.set()
                self.stop_event = None

            self.ten_env.log_info("Executing cancel_tts for bytedance_duplex")

            request_event_interval = 0
            if self.request_start_ts:
                request_event_interval = int(
                    (datetime.now() - self.request_start_ts).total_seconds()
                    * 1000
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
                self.total_audio_bytes = 0

                self.ten_env.log_info(
                    f"Sent tts_audio_end with INTERRUPTED reason for request_id: {self.current_request_id}"
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
