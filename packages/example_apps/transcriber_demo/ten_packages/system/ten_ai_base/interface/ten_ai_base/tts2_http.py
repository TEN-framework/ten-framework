#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from abc import abstractmethod
import asyncio
from datetime import datetime
import os
import traceback
from typing import Any, AsyncIterator, Tuple

from pydantic import BaseModel

from .helper import PCMWriter
from .message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from .struct import TTS2HttpResponseEventType, TTSTextInput
from .tts2 import AsyncTTS2BaseExtension, RequestState
from .const import LOG_CATEGORY_VENDOR, LOG_CATEGORY_KEY_POINT
from ten_runtime import AsyncTenEnv


class AsyncTTS2HttpConfig(BaseModel):
    dump: bool = False
    dump_path: str = "/tmp"

    @abstractmethod
    def update_params(self) -> None:
        raise NotImplementedError("update_params is not implemented")

    @abstractmethod
    def to_str(self, sensitive_handling: bool = True) -> str:
        raise NotImplementedError("to_str is not implemented")

    @abstractmethod
    def validate(self) -> None:
        raise NotImplementedError("validate is not implemented")

class AsyncTTS2HttpClient:
    @abstractmethod
    async def clean(self) -> None:
        raise NotImplementedError("clean is not implemented")

    @abstractmethod
    async def cancel(self) -> None:
        raise NotImplementedError("cancel is not implemented")

    @abstractmethod
    async def get(self, text: str, request_id: str) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        raise NotImplementedError("get is not implemented")

    @abstractmethod
    def get_extra_metadata(self) -> dict[str, Any]:
        raise NotImplementedError("get_extra_metadata is not implemented")


class AsyncTTS2HttpExtension(AsyncTTS2BaseExtension):
    @abstractmethod
    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        raise NotImplementedError("create_config is not implemented")


    @abstractmethod
    async def create_client(self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv) -> AsyncTTS2HttpClient:
        raise NotImplementedError("create_client is not implemented")

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: AsyncTTS2HttpConfig | None = None
        self.client: AsyncTTS2HttpClient | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.sent_ts: datetime | None = None
        self.request_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.first_chunk: bool = False
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # Store PCMWriter instances for different request_ids

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. Required parameter 'key' is missing."
                )

            self.config = await self.create_config(config_json_str)
            self.config.update_params()

            ten_env.log_info(
                f"LOG_CATEGORY_KEY_POINT: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.config.validate()

            self.client = await self.create_client(self.config, ten_env)

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
            try:
                await self.client.clean()
            except Exception as e:
                ten_env.log_warn(f"Error cleaning client: {traceback.format_exc()}")
            self.client = None

        # Clean up all PCMWriters
        # Create a copy of items to avoid issues if recorder_map is modified during iteration
        recorder_items = list(self.recorder_map.items())
        for request_id, recorder in recorder_items:
            # Double-check that the recorder still exists (may have been cleaned up by _send_audio_end_and_finish)
            if request_id not in self.recorder_map:
                ten_env.log_debug(
                    f"PCMWriter for request_id {request_id} was already cleaned up, skipping"
                )
                continue
            try:
                await recorder.flush()
                # Remove from map after successful flush
                if request_id in self.recorder_map:
                    del self.recorder_map[request_id]
                ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )
                # Still try to remove it even if flush failed
                if request_id in self.recorder_map:
                    del self.recorder_map[request_id]

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
                if self.request_ts:
                    await self._send_audio_end_and_finish(
                        request_id=self.current_request_id,
                        reason=TTSAudioEndReason.INTERRUPTED,
                    )
        else:
            self.ten_env.log_warn(
                "No current request found, skipping TTS cancellation."
            )

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
                    "TTS client is not initialized, attempting to reinitialize..."
                )
                self.client = await self.create_client(
                    config=self.config,
                    ten_env=self.ten_env,
                )
                self.ten_env.log_debug("TTS client reinitialized successfully.")

            self.ten_env.log_debug(
                f"current_request_id: {self.current_request_id}, new request_id: {t.request_id}, current_request_finished: {self.current_request_finished}"
            )
            if t.request_id != self.current_request_id:
                self.ten_env.log_debug(
                    f"New TTS request with ID: {t.request_id}"
                )
                self.first_chunk = True
                self.sent_ts = datetime.now()
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0  # Reset for new request
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

            # Get audio stream from TTS
            self.ten_env.log_debug(
                f"send_text_to_tts_server:  {t.text} of request_id: {t.request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            data = self.client.get(t.text, t.request_id)

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

                        # Send TTS audio start on first chunk
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
                                extra_metadata = self.client.get_extra_metadata()
                                await self.send_tts_ttfb_metrics(
                                    request_id=self.current_request_id,
                                    ttfb_ms=ttfb,
                                    extra_metadata=extra_metadata,
                                )
                                self.ten_env.log_debug(
                                    f"Sent TTS audio start and TTFB metrics: {ttfb}ms"
                                )
                            self.first_chunk = False

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
                            await self.recorder_map[
                                self.current_request_id
                            ].write(audio_chunk)

                        # Send audio data
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
                    # Send TTS audio end event
                    if self.request_ts and t.text_input_end:
                        await self._send_audio_end_and_finish(
                            request_id=self.current_request_id,
                            reason=TTSAudioEndReason.REQUEST_END,
                            log_message=f"Sent TTS audio end event, interval: {self._calculate_request_event_interval_ms()}ms, duration: {self._calculate_audio_duration_ms()}ms",
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
            
            # Handle case where loop ended normally but we didn't receive END event
            # This can happen if the stream ends without an explicit END event
            if self.request_ts and t.text_input_end and self.current_request_id:
                # Check if we haven't already sent audio_end (state should be FINALIZING)
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

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    def _calculate_request_event_interval_ms(self) -> int:
        """Calculate request event interval in milliseconds."""
        if self.request_ts:
            return int(
                (datetime.now() - self.request_ts).total_seconds() * 1000
            )
        return 0

    async def _send_audio_end_and_finish(
        self,
        request_id: str,
        reason: TTSAudioEndReason,
        log_message: str | None = None,
    ) -> None:
        """
        Send tts_audio_end event and finish the request.
        
        Args:
            request_id: The request ID
            reason: The reason for ending the audio
            log_message: Optional log message to display
        """
        request_event_interval = self._calculate_request_event_interval_ms()
        duration_ms = self._calculate_audio_duration_ms()
        
        if log_message:
            self.ten_env.log_debug(log_message)
        
        await self.send_tts_audio_end(
            request_id=request_id,
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=duration_ms,
            reason=reason,
        )
        
        # Close and clean up the recorder for this request_id
        if request_id in self.recorder_map:
            try:
                await self.recorder_map[request_id].flush()
                del self.recorder_map[request_id]
                self.ten_env.log_debug(
                    f"Closed and cleaned up PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                self.ten_env.log_error(
                    f"Error closing PCMWriter for request_id {request_id}: {e}"
                )
        
        await self.finish_request(
            request_id=request_id,
            reason=reason,
        )

    async def _handle_error_with_text_input_end(
        self,
        request_id: str,
        error: ModuleError,
        text_input_end: bool,
    ) -> None:
        """
        Handle error and send tts_audio_end if text_input_end was received.
        
        Args:
            request_id: The request ID
            error: The error to send
            text_input_end: Whether text_input_end was received
        """
        await self.send_tts_error(request_id=request_id, error=error)

        if text_input_end:
            # Send audio_end and finish
            await self._send_audio_end_and_finish(
                request_id=request_id,
                reason=TTSAudioEndReason.ERROR,
            )
        else:
            # Just finish the request without audio_end
            await self.finish_request(
                request_id=request_id,
                reason=TTSAudioEndReason.ERROR,
            )
