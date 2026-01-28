#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import copy
import itertools
import json
import os
import websockets
from dataclasses import asdict, dataclass
from ten_ai_base.message import ModuleMetrics
from typing import Any
from typing_extensions import override

from ten_ai_base.asr import (
    AsyncASRBaseExtension,
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
)

from ten_ai_base.const import (
    LOG_CATEGORY_VENDOR,
    LOG_CATEGORY_KEY_POINT,
)

from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleType,
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)

from ten_runtime import (
    AsyncTenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
    LogLevel,
)

from .config import BytedanceASRLLMConfig
from .volcengine_asr_client import VolcengineASRClient, ASRResponse, Utterance
from .log_id_dumper_manager import LogIdDumperManager
from .const import (
    DUMP_FILE_NAME,
    is_reconnectable_error,
)


@dataclass
class TwoPassDelayTracker:
    """Track two-pass delay metrics timestamps"""

    stream: int | None = None
    soft_vad: int | None = None

    def record_stream(self, timestamp: int) -> None:
        """Record stream result timestamp (always use the latest one)"""
        self.stream = timestamp

    def record_soft_vad(self, timestamp: int) -> None:
        """Record soft_vad two_pass result timestamp"""
        self.soft_vad = timestamp

    def calculate_metrics(
        self,
        current_timestamp: int,
        enable_nonstream: bool = False,
        enable_soft_vad: bool = False,
    ) -> dict[str, int]:
        metrics: dict[str, int] = {}
        # Only include two_pass_delay if enable_nonstream is True
        if enable_nonstream:
            metrics["two_pass_delay"] = (
                current_timestamp - self.stream
                if self.stream is not None
                else -1
            )
        # Only include soft_two_pass_delay if soft_vad is enabled
        # Send even if value is -1 (when soft_vad or stream is None)
        if enable_nonstream and enable_soft_vad:
            metrics["soft_two_pass_delay"] = (
                self.soft_vad - self.stream
                if self.soft_vad is not None and self.stream is not None
                else -1
            )
        return metrics

    def reset(self) -> None:
        self.stream = None
        self.soft_vad = None


class BytedanceASRLLMExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        # Connection state
        self.connected: bool = False
        self.client: VolcengineASRClient | None = None
        self.config: BytedanceASRLLMConfig | None = None
        self.last_finalize_timestamp: int = 0
        self.audio_dumper: Dumper | None = (
            None  # Original dumper, keep unchanged
        )
        self.vendor_result_dumper: Any = (
            None  # File handle for asr_vendor_result.jsonl
        )
        self.ten_env: AsyncTenEnv | None = None

        # Reconnection parameters
        self.min_retry_delay: float = 0.5
        self.max_retry_delay: float = 4.0  # Maximum delay between retries
        self.attempts: int = 0
        self.stopped: bool = False
        self.last_fatal_error: int | None = None
        self._reconnecting: bool = False

        # Session tracking
        self.session_id: str | None = None
        self.finalize_id: str | None = None

        # Audio timeline tracking (persists across reconnections)
        self.sent_user_audio_duration_ms_before_last_reset: int = 0

        # Two-pass delay metrics tracking
        self.two_pass_delay_tracker: TwoPassDelayTracker = TwoPassDelayTracker()

        # Log ID dumper manager
        self.log_id_dumper_manager: LogIdDumperManager | None = None

        # Enable utterance grouping
        self.enable_utterance_grouping: bool = True

    @override
    def vendor(self) -> str:
        """Get the name of the ASR vendor."""
        return "bytedance_bigmodel"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = BytedanceASRLLMConfig.model_validate_json(config_json)
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()

                # Initialize vendor result dumper
                vendor_result_dump_path = os.path.join(
                    self.config.dump_path, "asr_vendor_result.jsonl"
                )
                self.vendor_result_dumper = open(
                    vendor_result_dump_path, "a", encoding="utf-8"
                )
                # Initialize log_id_dumper_manager
                self.log_id_dumper_manager = LogIdDumperManager(
                    self.config, ten_env
                )

            self.audio_timeline.reset()
            self.enable_utterance_grouping = (
                self.config.get_enable_utterance_grouping()
            )

        except Exception as e:
            self.ten_env.log_error(f"Configuration error: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="CONFIG_ERROR",
                    message=f"Configuration validation failed: {str(e)}",
                ),
            )

    @override
    async def start_connection(self) -> None:
        """Start connection to Volcengine ASR service."""
        if not self.config:
            raise ValueError("Configuration not loaded")

        if self.config.get_auth_method() == "api_key":
            if not self.config.get_api_key():
                raise ValueError("api_key is required")
        else:
            if not self.config.get_app_key():
                raise ValueError("app_key is required")
            if not self.config.get_access_key():
                raise ValueError("access_key is required")

        try:
            self.client = VolcengineASRClient(
                url=self.config.get_api_url(),
                app_key=self.config.get_app_key(),
                access_key=self.config.get_access_key(),
                api_key=self.config.get_api_key(),
                auth_method=self.config.get_auth_method(),
                config=self.config,
                ten_env=self.ten_env,
            )

            # Set up callbacks
            self.client.set_on_result_callback(self._on_asr_result)
            self.client.set_on_connection_error_callback(
                self._on_connection_error
            )
            self.client.set_on_asr_error_callback(
                self._on_asr_communication_error
            )
            self.client.set_on_connected_callback(self._on_connected)
            self.client.set_on_disconnected_callback(self._on_disconnected)

            # Create dumper for new connection with UUID filename
            if self.log_id_dumper_manager:
                await self.log_id_dumper_manager.create_dumper()

            await self.client.connect()
            # Do NOT set self.connected = True here (callback-driven pattern)
            # Connection state will be set in _on_connected() callback when server confirms
            # This matches azure_asr_python pattern where state is set in event handler

        except Exception as e:
            self.ten_env.log_error(f"Failed to connect: {e}")
            self.connected = False
            # Don't raise the exception, let the extension continue
            # The connection will be retried later

    @override
    async def stop_connection(self) -> None:
        """Stop connection to Volcengine ASR service."""
        if self.client:
            try:
                await self.client.disconnect()
            except Exception as e:
                self.ten_env.log_error(f"Error during disconnect: {e}")
            finally:
                self.client = None
                self.connected = False

        # Stop log_id_dumper_manager if exists
        if self.log_id_dumper_manager:
            await self.log_id_dumper_manager.stop()

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        """Clean up resources when extension is deinitialized."""
        await super().on_deinit(ten_env)

        # Stop connection first to ensure proper cleanup order
        # This ensures client resources are cleaned up before other resources
        await self.stop_connection()

        if self.audio_dumper:
            try:
                await self.audio_dumper.stop()
            except Exception as e:
                ten_env.log_error(f"Error stopping audio dumper: {e}")
            finally:
                self.audio_dumper = None

        # log_id_dumper_manager is already stopped in stop_connection()
        # Keep temp file if not renamed (as per requirement)

        if self.vendor_result_dumper:
            try:
                self.vendor_result_dumper.close()
            except Exception as e:
                ten_env.log_error(f"Error closing vendor result dumper: {e}")
            finally:
                self.vendor_result_dumper = None

    @override
    def is_connected(self) -> bool:
        """Check if connected to ASR service."""
        # After finalize, connection may be closed by server (normal behavior)
        # Only check connection if we haven't finalized recently
        # if self.last_finalize_timestamp > 0:
        #     # Allow some time for final result to come back
        #     current_time = int(asyncio.get_event_loop().time() * 1000)
        #     if (
        #         current_time - self.last_finalize_timestamp
        #         < FINALIZE_GRACE_PERIOD_MS
        #     ):
        #         return True  # Still consider connected during finalize grace period

        return self.connected and self.client is not None

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """Send audio frame to ASR service."""
        if not self.is_connected():
            self.ten_env.log_warn(
                "Not connected to ASR service, attempting to reconnect..."
            )
            try:
                await self.start_connection()
                if not self.is_connected():
                    self.ten_env.log_error("Failed to reconnect to ASR service")
                    return False
            except Exception as e:
                self.ten_env.log_error(f"Failed to reconnect: {e}")
                return False

        buf = frame.lock_buf()
        try:
            # Update session_id if changed
            if self.session_id != session_id:
                self.session_id = session_id

            # Get audio data from frame
            audio_data = bytes(buf)

            # Dump audio if enabled (original audio_dumper, unchanged)
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)

            # Dump audio to log_id_dumper if enabled (manager handles rename if needed)
            if self.log_id_dumper_manager:
                await self.log_id_dumper_manager.push_bytes(audio_data)

            self.audio_timeline.add_user_audio(
                int(len(buf) / (self.input_audio_sample_rate() / 1000 * 2))
            )

            # Send audio to ASR service
            await self.client.send_audio(audio_data)
            return True

        except Exception as e:
            self.ten_env.log(LogLevel.ERROR, f"Error sending audio: {e}")
            await self._handle_error(e)
            return False
        finally:
            frame.unlock_buf(buf)

    @override
    async def finalize(self, session_id: str | None) -> None:
        """Finalize current ASR session."""
        if not self.is_connected():
            return

        try:
            self.last_finalize_timestamp = int(
                asyncio.get_event_loop().time() * 1000
            )
            self.ten_env.log_debug(
                f"Finalize start at {self.last_finalize_timestamp}"
            )

            await self.client.finalize()

            # Record silence audio in timeline (client sends silence data)
            if self.config:
                self.audio_timeline.add_silence_audio(
                    self.config.get_mute_pkg_duration_ms()
                )
        except Exception as e:
            self.ten_env.log_error(f"Error finalizing session: {e}")
            await self._handle_error(e)

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        """Get required input audio sample rate."""
        return self.config.get_sample_rate() if self.config else 16000

    @override
    def input_audio_channels(self) -> int:
        """Get the number of audio channels for input."""
        return self.config.get_channel() if self.config else 1

    @override
    def input_audio_sample_width(self) -> int:
        """Get the sample width in bytes for input audio."""
        return self.config.get_bits() // 8 if self.config else 2

    async def _handle_error(self, error: Exception) -> None:
        """Handle ASR errors."""
        error_code = getattr(error, "code", ModuleErrorCode.FATAL_ERROR.value)

        # Always send error regardless of whether it's reconnectable
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=str(error),
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(error_code),
                message=str(error),
            ),
        )

        # If error is reconnectable and not stopped, attempt reconnection
        if is_reconnectable_error(error_code) and not self.stopped:
            await self._handle_reconnect()

    async def _handle_reconnect(self) -> None:
        """Handle reconnection logic with exponential backoff (min delay: 0.5s, max delay: max_retry_delay).

        - First retry: 0.5s delay
        - Subsequent retries: exponential backoff (base 0.5s) with min 0.5s and cap at max_retry_delay
        - Unlimited retries unless stopped
        """
        if self._reconnecting:
            return

        self._reconnecting = True

        try:
            self.attempts += 1

            # Calculate delay with exponential backoff
            # First attempt: 0.5s delay
            # Subsequent attempts: exponential backoff (base 0.5s) with min 0.5s and cap at max_retry_delay
            if self.attempts == 1:
                delay = self.min_retry_delay
            else:
                delay = self.min_retry_delay * (2 ** (self.attempts - 2))
                delay = max(
                    self.min_retry_delay, min(delay, self.max_retry_delay)
                )

            self.ten_env.log_info(
                f"Reconnecting... Attempt {self.attempts}, delay: {delay:.2f}s"
            )

            if delay > 0:
                await asyncio.sleep(delay)

            try:
                await self.stop_connection()
                await self.start_connection()
            except Exception as e:
                self.ten_env.log_error(f"Reconnection failed: {e}")
                if not self.stopped:
                    await self._handle_reconnect()
        finally:
            self._reconnecting = False

    def _build_metadata_with_asr_info(
        self,
        base_metadata: dict[str, Any] | None = None,
        additional_fields: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        """Build metadata according to protocol: session_id at root, others in asr_info.

        Args:
            base_metadata: Base metadata dict (defaults to self.metadata if None)
            additional_fields: Additional fields to add to asr_info

        Returns:
            Metadata dict with structure: {"session_id": "...", "asr_info": {...}}
        """
        # Start with a copy of base metadata if available
        if base_metadata is None:
            base_metadata = (
                copy.deepcopy(self.metadata)
                if self.metadata is not None
                else {}
            )

        # Extract session_id from base metadata if present
        session_id = base_metadata.pop("session_id", None)

        # Collect all other fields into asr_info
        asr_info = copy.deepcopy(base_metadata)

        # Add vendor field to asr_info
        asr_info["vendor"] = self.vendor()

        # Add additional fields to asr_info if provided
        if additional_fields:
            asr_info.update(additional_fields)

        # Build final metadata structure
        metadata: dict[str, Any] = {}
        if session_id is not None:
            metadata["session_id"] = session_id
        metadata["asr_info"] = asr_info

        return metadata

    def _extract_final_result_metadata(
        self, utterance: Utterance
    ) -> dict[str, Any]:
        """Extract metadata from utterance additions.

        According to asr-interface.json protocol:
        - session_id should be at metadata root level
        - All other fields should be in metadata.asr_info
        """
        # Get utterance additions as additional fields
        additional_fields = None
        if utterance.additions and isinstance(utterance.additions, dict):
            additional_fields = utterance.additions

        return self._build_metadata_with_asr_info(
            additional_fields=additional_fields
        )

    def _extract_non_final_result_metadata(
        self, utterance: Utterance
    ) -> dict[str, Any]:
        """Extract metadata from utterance additions for non-final results.

        For non-final results (stream results), only extract invoke_type and source
        to distinguish between soft_vad, hard_vad, and stream.

        According to asr-interface.json protocol:
        - session_id should be at metadata root level
        - All other fields should be in metadata.asr_info
        """
        # Extract only invoke_type and source from utterance additions
        additional_fields = None
        if utterance.additions and isinstance(utterance.additions, dict):
            additions = utterance.additions
            additional_fields = {}
            if "invoke_type" in additions:
                additional_fields["invoke_type"] = additions["invoke_type"]
            if "source" in additions:
                additional_fields["source"] = additions["source"]

        return self._build_metadata_with_asr_info(
            additional_fields=additional_fields
        )

    def _calculate_utterance_start_ms(
        self, utterance_start_time_ms: int
    ) -> int:
        """Calculate actual start_ms for an utterance based on its start_time."""
        return int(
            self.audio_timeline.get_audio_duration_before_time(
                utterance_start_time_ms
            )
            + self.sent_user_audio_duration_ms_before_last_reset
        )

    async def _send_two_pass_delay_metrics(
        self, current_timestamp: int
    ) -> None:
        """Send two-pass delay metrics via ModuleMetrics.

        Calculates and sends:
        - two_pass_delay: delay from stream result to hard_vad two_pass result
        - soft_two_pass_delay: delay from stream result to soft_vad two_pass result
        """
        # Check if enable_nonstream and soft_vad are enabled in request config
        enable_nonstream = False
        enable_soft_vad = False
        if self.config and self.config.get_request_config():
            # Check if enable_nonstream is True in request params
            enable_nonstream = self.config.get_request_config().get(
                "enable_nonstream", False
            )
            # Check if soft_vad_window_size exists in request params
            enable_soft_vad = (
                "soft_vad_window_size" in self.config.get_request_config()
            )

        vendor_metrics = self.two_pass_delay_tracker.calculate_metrics(
            current_timestamp,
            enable_nonstream=enable_nonstream,
            enable_soft_vad=enable_soft_vad,
        )

        await self._send_asr_metrics(
            ModuleMetrics(
                module=ModuleType.ASR,
                vendor=self.vendor(),
                metrics=vendor_metrics,
            )
        )

    async def _send_asr_result_from_text(
        self,
        text: str,
        is_final: bool,
        start_ms: int,
        duration_ms: int,
        language: str,
        metadata: dict[str, Any],
    ) -> None:
        """Send ASR result with given text and metadata."""
        asr_result = ASRResult(
            text=text,
            final=is_final,
            start_ms=start_ms,
            duration_ms=duration_ms,
            language=language,
            words=[],
            metadata=metadata,
        )
        await self.send_asr_result(asr_result)

    async def _track_utterance_timestamps(self, result: ASRResponse) -> None:
        """Track utterance timestamps and send two-pass delay metrics."""
        current_timestamp = int(asyncio.get_event_loop().time() * 1000)
        for utterance in result.utterances:
            # Skip utterances with invalid timestamps
            if utterance.start_time == -1 or utterance.end_time == -1:
                self.ten_env.log_warn(
                    f"Skipping utterance with invalid timestamps: {utterance.text}"
                )
                continue

            # Skip empty utterances
            if not utterance.text.strip():
                continue

            # Identify result type and record timestamps
            additions = utterance.additions if utterance.additions else {}
            source = additions.get("source", "")
            invoke_type = additions.get("invoke_type", "")
            is_final = utterance.definite

            # Record timestamps using tracker
            match (source, invoke_type, is_final):
                case ("stream", _, _):
                    self.two_pass_delay_tracker.record_stream(current_timestamp)
                case ("two_pass", "soft_vad", _):
                    self.two_pass_delay_tracker.record_soft_vad(
                        current_timestamp
                    )
                case ("two_pass", "hard_vad", True):
                    await self._send_two_pass_delay_metrics(current_timestamp)
                case _:
                    pass  # Other combinations don't need timestamp recording

    async def _on_asr_result(self, result: ASRResponse) -> None:
        """Handle ASR result from client."""
        try:
            # Extract log_id from result.additions.log_id (only process the first one)
            if (
                result.result
                and isinstance(result.result, dict)
                and self.log_id_dumper_manager
            ):
                additions = result.result.get("additions")
                if additions and isinstance(additions, dict):
                    log_id = additions.get("log_id")
                    if log_id and isinstance(log_id, str):
                        await self.log_id_dumper_manager.update_log_id(log_id)

            # Dump vendor result if enabled
            if self.vendor_result_dumper:
                try:
                    # Get original JSON from payload_msg or construct from result
                    if result.payload_msg:
                        vendor_result_json = json.dumps(
                            result.payload_msg, ensure_ascii=False
                        )
                    else:
                        # Fallback: construct from ASRResponse
                        vendor_result_json = json.dumps(
                            asdict(result), ensure_ascii=False
                        )
                    self.vendor_result_dumper.write(vendor_result_json + "\n")
                    self.vendor_result_dumper.flush()
                except Exception as e:
                    self.ten_env.log_error(f"Error dumping vendor result: {e}")

            # Check if this is an error response
            if result.code != 0:
                # This is an ASR error response, handle it through send_asr_error
                error_message = "Unknown ASR error"
                if result.payload_msg and "error_message" in result.payload_msg:
                    error_message = result.payload_msg["error_message"]

                await self.send_asr_error(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message=error_message,
                    ),
                    ModuleErrorVendorInfo(
                        vendor=self.vendor(),
                        code=str(result.code),
                        message=error_message,
                    ),
                )
                return

            # Create ASR result data for successful response
            if result.payload_msg:
                full_json = json.dumps(result.payload_msg, ensure_ascii=False)
            else:
                full_json = "{}"
            self.ten_env.log_debug(
                f"vendor_result: on_recognized: {result.text}, language: {result.language}, full_json: {full_json}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Process utterances: send definite=true individually,
            # and concatenate adjacent definite=false utterances together
            if not result.utterances:
                # No utterances, send result.text as fallback (non-final)
                # Use result.start_ms for fallback case
                actual_start_ms = self._calculate_utterance_start_ms(
                    result.start_ms
                )
                # Build metadata according to protocol: session_id at root, others in asr_info
                metadata = self._build_metadata_with_asr_info()
                await self._send_asr_result_from_text(
                    text=result.text,
                    is_final=False,
                    start_ms=actual_start_ms,
                    duration_ms=result.duration_ms,
                    language=result.language,
                    metadata=metadata,
                )
                return

            has_final_result = False

            await self._track_utterance_timestamps(result)

            if not self.enable_utterance_grouping:
                for utterance in result.utterances:
                    # Skip utterances with invalid timestamps
                    if utterance.start_time == -1 or utterance.end_time == -1:
                        self.ten_env.log_warn(
                            f"Skipping utterance with invalid timestamps: {utterance.text}"
                        )
                        continue

                    # Skip empty utterances
                    if not utterance.text.strip():
                        continue

                    is_final = utterance.definite
                    # Calculate start_ms and duration_ms for this utterance
                    actual_start_ms = self._calculate_utterance_start_ms(
                        utterance.start_time
                    )
                    duration_ms = utterance.end_time - utterance.start_time

                    # Extract metadata (always include invoke_type and source for all results)
                    if is_final:
                        has_final_result = True
                        metadata = self._extract_final_result_metadata(
                            utterance
                        )
                    else:
                        metadata = self._extract_non_final_result_metadata(
                            utterance
                        )

                    await self._send_asr_result_from_text(
                        text=utterance.text,
                        is_final=is_final,
                        start_ms=actual_start_ms,
                        duration_ms=duration_ms,
                        language=result.language,
                        metadata=metadata,
                    )
            else:
                # Filter out invalid utterances first
                valid_utterances = [
                    u
                    for u in result.utterances
                    if u.start_time != -1
                    and u.end_time != -1
                    and u.text.strip()
                ]

                # Log warnings for invalid utterances
                for utterance in result.utterances:
                    if utterance.start_time == -1 or utterance.end_time == -1:
                        self.ten_env.log_warn(
                            f"Skipping utterance with invalid timestamps: {utterance.text}"
                        )

                # Group and merge consecutive utterances with the same definite value
                merged_utterances = []
                for is_final, group in itertools.groupby(
                    valid_utterances, key=lambda u: u.definite
                ):
                    group_list = list(group)
                    if not group_list:
                        continue

                    first, last = group_list[0], group_list[-1]
                    merged_utterances.append(
                        {
                            "text": "".join(u.text for u in group_list),
                            "is_final": is_final,
                            "start_time": first.start_time,
                            "duration_ms": last.end_time - first.start_time,
                            "metadata": (
                                self._extract_final_result_metadata(last)
                                if is_final
                                else self._extract_non_final_result_metadata(
                                    last
                                )
                            ),
                            "utterance": last,  # Keep reference for timestamp tracking
                        }
                    )

                # Process merged utterances
                for merged in merged_utterances:
                    is_final = merged["is_final"]

                    # Calculate start_ms for merged utterance
                    actual_start_ms = self._calculate_utterance_start_ms(
                        merged["start_time"]
                    )

                    if is_final:
                        has_final_result = True

                    await self._send_asr_result_from_text(
                        text=merged["text"],
                        is_final=is_final,
                        start_ms=actual_start_ms,
                        duration_ms=merged["duration_ms"],
                        language=result.language,
                        metadata=merged["metadata"],
                    )

            # finalize end signal if there was any final result
            if has_final_result:
                await self._finalize_end()

        except Exception as e:
            self.ten_env.log_error(f"Error handling ASR result: {e}")

    async def _finalize_end(self) -> None:
        """Handle finalization end logic."""
        if self.last_finalize_timestamp != 0:
            self.last_finalize_timestamp = 0
            # Send asr_finalize_end signal
            await self.send_asr_finalize_end()

            # Reset two-pass delay metrics tracking for next recognition
            self.two_pass_delay_tracker.reset()

            # After finalize end, connection is expected to be closed by server
            # This is normal behavior, so we don't need to reconnect

    async def _on_asr_error(self, error_code: int, error_message: str) -> None:
        """Handle ASR error from client."""
        self.ten_env.log_error(
            f"vendor_error: code: {error_code}, reason: {error_message}",
            category=LOG_CATEGORY_VENDOR,
        )

        # Always send error regardless of whether it's reconnectable
        module_error = ModuleError(
            module=ModuleType.ASR,
            code=ModuleErrorCode.NON_FATAL_ERROR.value,
            message=error_message,
        )
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(),
            code=str(error_code),
            message=error_message,
        )
        await self.send_asr_error(module_error, vendor_info)

        # If error is reconnectable and not stopped, attempt reconnection
        if is_reconnectable_error(error_code) and not self.stopped:
            await self._handle_reconnect()

    def _on_connection_error(self, exception: Exception) -> None:
        """Handle connection-level errors (HTTP stage)."""
        # Connection error handling logic
        error_message = str(exception)

        # Extract HTTP error code directly from exception message if available
        error_code = 0  # Default to 0 for unknown errors

        # Try to extract HTTP status code from error message
        if "HTTP" in error_message:
            import re

            http_match = re.search(r"HTTP (\d+)", error_message)
            if http_match:
                error_code = int(http_match.group(1))

        # Create task to report error to TEN framework
        asyncio.create_task(self._on_asr_error(error_code, error_message))

    def _on_asr_communication_error(self, exception: Exception) -> None:
        """Handle ASR communication errors (WebSocket stage)."""
        # Check if this is a server error response with a specific error code
        if hasattr(exception, "code"):
            # This is a server error response (like ServerErrorResponse)
            # Keep the original error code for proper retry logic
            error_code = getattr(
                exception, "code", ModuleErrorCode.NON_FATAL_ERROR.value
            )
            error_message = str(exception)
        elif isinstance(exception, websockets.exceptions.ConnectionClosed):
            # Connection closed - this might be retryable depending on context
            error_code = ModuleErrorCode.NON_FATAL_ERROR.value
            error_message = str(exception)
        elif isinstance(exception, websockets.exceptions.InvalidMessage):
            # Invalid message format - might be retryable
            error_code = ModuleErrorCode.NON_FATAL_ERROR.value
            error_message = str(exception)
        elif isinstance(exception, websockets.exceptions.WebSocketException):
            # General WebSocket error - might be retryable
            error_code = ModuleErrorCode.NON_FATAL_ERROR.value
            error_message = str(exception)
        else:
            # Unknown exception - default to non fatal
            error_code = ModuleErrorCode.NON_FATAL_ERROR.value
            error_message = str(exception)

        asyncio.create_task(self._on_asr_error(error_code, error_message))

    def _on_asr_exception(self, exception: Exception) -> None:
        """Handle connection-level exceptions from client (adapter for error_callback)."""
        error_code = getattr(exception, "code", None)

        if error_code is None:
            # Map connection exceptions to appropriate ModuleErrorCode values
            if isinstance(exception, ConnectionError):
                error_code = ModuleErrorCode.FATAL_ERROR.value
            elif isinstance(exception, TimeoutError):
                error_code = ModuleErrorCode.FATAL_ERROR.value
            elif isinstance(exception, ValueError):
                error_code = ModuleErrorCode.FATAL_ERROR.value
            else:
                error_code = ModuleErrorCode.FATAL_ERROR.value

        error_message = str(exception)
        asyncio.create_task(self._on_asr_error(error_code, error_message))

    def _initialize_connection_state(self) -> None:
        """Initialize connection state after server confirmation.

        This method performs initialization that was previously in start_connection():
        - Updates sent_user_audio_duration_ms_before_last_reset
        - Resets audio timeline
        - Resets retry attempts counter
        - Logs connection success
        """
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        self.ten_env.log_info(
            f"sent_user_audio_duration_ms_before_last_reset: {self.sent_user_audio_duration_ms_before_last_reset}"
        )

        self.attempts = 0  # Reset retry attempts on successful connection

        self.ten_env.log_info(
            "Successfully connected to Volcengine ASR service"
        )

    def _on_connected(self) -> None:
        """Handle connection established (callback-driven state change).

        Called by client when server sends MESSAGE_TYPE_SERVER_FULL_RESPONSE with code=0.
        This matches azure_asr_python pattern where _azure_event_handler_on_session_started()
        sets connected=True.
        """
        self.ten_env.log_info(
            f"vendor_status_changed: session_id: {self.session_id}",
            category=LOG_CATEGORY_VENDOR,
        )

        # Set connected=True here (callback-driven pattern)
        self.connected = True

        # Perform initialization that was previously in start_connection()
        self._initialize_connection_state()

    def _on_disconnected(self) -> None:
        """Handle connection lost."""
        self.ten_env.log_info(
            f"vendor_status_changed: session_id: {self.session_id}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.connected = False

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        """Handle commands."""
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)
