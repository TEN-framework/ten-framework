#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback
from typing import Any

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_VENDOR, LOG_CATEGORY_KEY_POINT
from .config import CartesiaTTSConfig

from .cartesia_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    EVENT_TTS_ERROR,
    CartesiaTTSClient,
    CartesiaTTSConnectionException,
)
from ten_runtime import AsyncTenEnv


class CartesiaTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: CartesiaTTSConfig | None = None
        self.client: CartesiaTTSClient | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # Store PCMWriter instances for different request_ids

    @staticmethod
    def _clamp_float(value: Any, lower: float, upper: float) -> float | None:
        """Clamp value into [lower, upper] returning None when not a number."""
        if value is None:
            return None

        try:
            numeric = float(value)
        except (TypeError, ValueError):
            return None

        if numeric < lower:
            return lower
        if numeric > upper:
            return upper
        return numeric

    @staticmethod
    def _format_ratio(value: float) -> str:
        """Format ratios while trimming trailing zeros for cleaner SSML."""
        return f"{value:.3f}".rstrip("0").rstrip(".")

    @staticmethod
    def _normalize_time_string(value: Any) -> str | None:
        if value is None:
            return None

        if isinstance(value, str):
            stripped = value.strip()
            return stripped or None

        try:
            stripped = str(value).strip()
        except Exception:
            return None

        return stripped or None

    def _compose_ssml_text(
        self, text: str, metadata: dict[str, Any] | None
    ) -> str:
        """Compose SSML directives based on config/metadata overrides."""
        if not self.config:
            return text

        overrides: dict[str, Any] = {}
        if metadata:
            overrides_candidate = metadata.get("ssml") or metadata.get(
                "ssml_tags"
            )
            if isinstance(overrides_candidate, dict):
                overrides = overrides_candidate

        ssml_cfg = self.config.ssml
        enabled = overrides.get("enabled", ssml_cfg.enabled)
        if not enabled:
            return text

        speed_ratio = overrides.get("speed_ratio", ssml_cfg.speed_ratio)
        speed_ratio = self._clamp_float(speed_ratio, 0.6, 1.5)

        volume_ratio = overrides.get("volume_ratio", ssml_cfg.volume_ratio)
        volume_ratio = self._clamp_float(volume_ratio, 0.5, 2.0)

        emotion = overrides.get("emotion", ssml_cfg.emotion)
        if isinstance(emotion, str):
            emotion = emotion.strip() or None
        else:
            emotion = None

        pre_break_time = overrides.get(
            "pre_break_time", ssml_cfg.pre_break_time
        )
        pre_break_time = self._normalize_time_string(pre_break_time)

        post_break_time = overrides.get(
            "post_break_time", ssml_cfg.post_break_time
        )
        post_break_time = self._normalize_time_string(post_break_time)

        spell_words_override = overrides.get("spell_words")
        if spell_words_override is not None:
            if isinstance(spell_words_override, str):
                spell_words = [spell_words_override]
            elif isinstance(spell_words_override, (list, tuple, set)):
                spell_words = list(spell_words_override)
            else:
                spell_words = []
        else:
            spell_words = list(ssml_cfg.spell_words)

        cleaned_spell_words: list[str] = []
        for word in spell_words:
            if isinstance(word, str):
                trimmed = word.strip()
                if trimmed and trimmed not in cleaned_spell_words:
                    cleaned_spell_words.append(trimmed)

        mutated_text = text
        if cleaned_spell_words:
            # Replace longest words first for deterministic wrapping.
            for word in sorted(cleaned_spell_words, key=len, reverse=True):
                mutated_text = mutated_text.replace(
                    word, f"<spell>{word}</spell>"
                )

        prefix_tags: list[str] = []
        if pre_break_time:
            prefix_tags.append(f'<break time="{pre_break_time}"/>')
        if speed_ratio is not None and abs(speed_ratio - 1.0) > 1e-3:
            prefix_tags.append(
                f'<speed ratio="{self._format_ratio(speed_ratio)}"/>'
            )
        if volume_ratio is not None and abs(volume_ratio - 1.0) > 1e-3:
            prefix_tags.append(
                f'<volume ratio="{self._format_ratio(volume_ratio)}"/>'
            )
        if emotion:
            prefix_tags.append(f'<emotion value="{emotion}"/>')

        prefix = " ".join(prefix_tags)
        if prefix:
            mutated_text = f"{prefix} {mutated_text}".strip()

        if post_break_time:
            mutated_text = f'{mutated_text} <break time="{post_break_time}"/>'

        return mutated_text

    def _apply_ssml_tags_safe(
        self, text: str, metadata: dict[str, Any] | None
    ) -> str:
        """Wrapper that guards SSML composition errors."""
        try:
            mutated = self._compose_ssml_text(text, metadata)
            if mutated != text:
                self.ten_env.log_debug(
                    f"Applied SSML tags: {mutated[:500]}",
                    category=LOG_CATEGORY_VENDOR,
                )
            return mutated
        except Exception as exc:  # pragma: no cover - defensive path
            self.ten_env.log_error(
                f"Failed to compose SSML tags: {exc}",
                category=LOG_CATEGORY_VENDOR,
            )
            return text

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")
            ten_env.log_info(f"config_json_str: {config_json_str}")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. Required parameter 'key' is missing."
                )

            self.config = CartesiaTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()
            ten_env.log_info(
                f"LOG_CATEGORY_KEY_POINT: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if not self.config.api_key:
                raise ValueError("API key is required")

            self.client = CartesiaTTSClient(
                config=self.config,
                ten_env=ten_env,
                send_fatal_tts_error=self.send_fatal_tts_error,
                send_non_fatal_tts_error=self.send_non_fatal_tts_error,
            )
            asyncio.create_task(self.client.start())
            ten_env.log_debug(
                "CartesiaTTSWebsocket client initialized successfully"
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            await self.client.stop()
            self.client = None

        # Clean up all PCMWriters
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_debug(
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

    async def cancel_tts(self) -> None:
        self.current_request_finished = True
        if self.current_request_id:
            self.ten_env.log_debug(
                f"Current request {self.current_request_id} is being cancelled. Sending INTERRUPTED."
            )

            if self.client:
                await self.client.cancel()
                if self.sent_ts:
                    request_event_interval = int(
                        (datetime.now() - self.sent_ts).total_seconds() * 1000
                    )
                    duration_ms = self._calculate_audio_duration_ms()
                    await self.send_tts_audio_end(
                        request_id=self.current_request_id,
                        request_event_interval_ms=request_event_interval,
                        request_total_audio_duration_ms=duration_ms,
                        reason=TTSAudioEndReason.INTERRUPTED,
                    )
        else:
            self.ten_env.log_warn(
                "No current request found, skipping TTS cancellation."
            )

    def vendor(self) -> str:
        return "cartesia"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}",
            )
            # If client is None, it means the connection was dropped or never initialized.
            # Attempt to re-establish the connection.
            if self.client is None:
                self.ten_env.log_debug(
                    "TTS client is not initialized, attempting to reconnect..."
                )
                self.client = CartesiaTTSClient(
                    config=self.config,
                    ten_env=self.ten_env,
                    send_fatal_tts_error=self.send_fatal_tts_error,
                    send_non_fatal_tts_error=self.send_non_fatal_tts_error,
                )
                asyncio.create_task(self.client.start())
                self.ten_env.log_debug("TTS client reconnected successfully.")

            self.ten_env.log_debug(
                f"current_request_id: {self.current_request_id}, new request_id: {t.request_id}, current_request_finished: {self.current_request_finished}"
            )

            if t.request_id != self.current_request_id:
                self.ten_env.log_debug(
                    f"New TTS request with ID: {t.request_id}"
                )
                self.client.reset_ttfb()
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0  # Reset for new request
                self.sent_ts = None
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)
                # Create new PCMWriter for new request_id and clean up old ones
                if self.config and self.config.dump:
                    # Clean up old PCMWriters (except current request_id)
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

                    # Create new PCMWriter
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"cartesia_dump_{t.request_id}.pcm",
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
                    f"KEYPOINT finish session for request ID: {t.request_id}"
                )
                self.current_request_finished = True

            prepared_text = self._apply_ssml_tags_safe(t.text, t.metadata)

            if prepared_text.strip() != "":
                # Get audio stream from Cartesia TTS
                self.ten_env.log_debug(
                    f"send_text_to_tts_server:  {prepared_text} of request_id: {t.request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )
                data = self.client.get(prepared_text)

                chunk_count = 0
                if self.sent_ts is None:
                    self.sent_ts = datetime.now()
                async for data_msg, event_status in data:
                    self.ten_env.log_debug(
                        f"Received event_status: {event_status}"
                    )
                    if event_status == EVENT_TTS_RESPONSE:
                        if (
                            data_msg is not None
                            and isinstance(data_msg, bytes)
                            and len(data_msg) > 0
                        ):
                            chunk_count += 1
                            self.total_audio_bytes += len(data_msg)
                            self.ten_env.log_info(
                                f"Received audio chunk #{chunk_count}, size: {len(data_msg)} bytes"
                            )
                            # Write to dump file if enabled
                            if (
                                self.config
                                and self.config.dump
                                and self.current_request_id
                                and self.current_request_id in self.recorder_map
                            ):
                                self.ten_env.log_debug(
                                    f"Writing audio chunk to dump file, dump url: {self.config.dump_path}"
                                )
                                asyncio.create_task(
                                    self.recorder_map[
                                        self.current_request_id
                                    ].write(data_msg)
                                )

                            # Send audio data
                            await self.send_tts_audio_data(data_msg)
                        else:
                            self.ten_env.log_debug(
                                "Received empty payload for TTS response"
                            )
                            if t.text_input_end:
                                duration_ms = (
                                    self._calculate_audio_duration_ms()
                                )
                                request_event_interval = (
                                    self._current_request_interval_ms()
                                )
                                await self.send_tts_audio_end(
                                    request_id=self.current_request_id,
                                    request_event_interval_ms=request_event_interval,
                                    request_total_audio_duration_ms=duration_ms,
                                )
                                self.sent_ts = None
                                self.ten_env.log_debug(
                                    f"Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                                )
                    elif event_status == EVENT_TTS_TTFB_METRIC:
                        if data_msg is not None and isinstance(data_msg, int):
                            self.sent_ts = datetime.now()
                            ttfb = data_msg
                            await self.send_tts_audio_start(
                                request_id=self.current_request_id,
                            )
                            extra_metadata = {
                                "model_id": self.config.params.get(
                                    "model_id", ""
                                ),
                                "voice_id": self.config.params.get(
                                    "voice", {}
                                ).get("id", ""),
                            }
                            await self.send_tts_ttfb_metrics(
                                request_id=self.current_request_id,
                                ttfb_ms=ttfb,
                                extra_metadata=extra_metadata,
                            )

                            self.ten_env.log_debug(
                                f"Sent TTS audio start and TTFB metrics: {ttfb}ms"
                            )
                    elif event_status == EVENT_TTS_END:
                        self.ten_env.log_info(
                            "Received TTS_END event from Cartesia TTS"
                        )
                        # Send TTS audio end event
                        if t.text_input_end:
                            request_event_interval = (
                                self._current_request_interval_ms()
                            )
                            duration_ms = self._calculate_audio_duration_ms()
                            await self.send_tts_audio_end(
                                request_id=self.current_request_id,
                                request_event_interval_ms=request_event_interval,
                                request_total_audio_duration_ms=duration_ms,
                            )
                            self.sent_ts = None
                            self.ten_env.log_debug(
                                f"Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                            )
                        break
                    elif event_status == EVENT_TTS_ERROR:
                        self.ten_env.log_error(
                            "Received TTS_ERROR event from Cartesia TTS"
                        )
                        # Send TTS audio end event
                        if t.text_input_end:
                            request_event_interval = (
                                self._current_request_interval_ms()
                            )
                            duration_ms = self._calculate_audio_duration_ms()
                            await self.send_tts_audio_end(
                                request_id=self.current_request_id,
                                request_event_interval_ms=request_event_interval,
                                request_total_audio_duration_ms=duration_ms,
                            )
                            self.sent_ts = None
                            self.ten_env.log_debug(
                                f"Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                            )
                        break

                self.ten_env.log_debug(
                    f"TTS processing completed, total chunks: {chunk_count}"
                )
            elif t.text_input_end:
                duration_ms = self._calculate_audio_duration_ms()
                request_event_interval = self._current_request_interval_ms()
                await self.send_tts_audio_end(
                    request_id=self.current_request_id,
                    request_event_interval_ms=request_event_interval,
                    request_total_audio_duration_ms=duration_ms,
                )
                self.sent_ts = None
                self.ten_env.log_debug(
                    f"Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                )

        except CartesiaTTSConnectionException as e:
            self.ten_env.log_error(
                f"CartesiaTTSConnectionException in request_tts: {e.body}. text: {t.text}"
            )

            if e.status_code == 401:
                await self.send_tts_error(
                    request_id=self.current_request_id,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )
            else:
                await self.send_tts_error(
                    request_id=self.current_request_id,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                request_id=self.current_request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
            # When a connection error occurs, destroy the client instance.
            # It will be recreated on the next request.
            if isinstance(e, ConnectionRefusedError) and self.client:
                await self.client.stop()
                self.client = None
                self.ten_env.log_debug(
                    "Client connection dropped, instance destroyed. Will attempt to reconnect on next request."
                )

    async def send_fatal_tts_error(self, error_message: str) -> None:
        await self.send_tts_error(
            request_id=self.current_request_id or "",
            error=ModuleError(
                message=error_message,
                module=ModuleType.TTS,
                code=ModuleErrorCode.FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )

    async def send_non_fatal_tts_error(self, error_message: str) -> None:
        await self.send_tts_error(
            request_id=self.current_request_id or "",
            error=ModuleError(
                message=error_message,
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )

    def _current_request_interval_ms(self) -> int:
        if not self.sent_ts:
            return 0
        return int((datetime.now() - self.sent_ts).total_seconds() * 1000)

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)
