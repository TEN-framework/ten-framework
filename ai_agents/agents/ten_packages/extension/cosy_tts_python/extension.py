#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback

from websocket import WebSocketConnectionClosedException
from ten_ai_base.helper import generate_file_name, PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension, DATA_FLUSH
from ten_runtime import AsyncTenEnv

from .config import CosyTTSConfig
from .cosy_tts import (
    MESSAGE_TYPE_PCM,
    MESSAGE_TYPE_CMD_ERROR,
    MESSAGE_TYPE_CMD_CANCEL,
    CosyTTSClient,
)


class CosyTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

        # TTS client for Cosy TTS service
        self.client: CosyTTSClient | None = None
        # Configuration for TTS settings
        self.config: CosyTTSConfig | None = None
        # Flag indicating if current request is finished
        self.current_request_finished: bool = False
        # ID of the current TTS request being processed
        self.current_request_id: str | None = None
        # Turn ID for conversation tracking
        self.current_turn_id: int = -1
        # Extension name for logging and identification
        self.name: str = name
        # Store PCMWriter instances for different request_ids
        self.recorder_map: dict[str, PCMWriter] = {}
        # Timestamp when TTS request was sent to service
        self.request_start_ts: datetime | None = None
        # Total audio duration for current request in milliseconds
        self.request_total_audio_duration_ms: int | None = None
        # Time to first byte for current request in milliseconds
        self.request_ttfb: int | None = None
        # Total audio bytes received for current request
        self.total_audio_bytes: int = 0
        # Background task for processing audio data
        self.audio_processor_task: asyncio.Task | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = CosyTTSConfig.model_validate_json(config_json)
                # Update params from config
                self.config.update_params()

                self.ten_env.log_info(
                    f"KEYPOINT config: {self.config.to_str()}"
                )

                # Validate params
                self.config.validate_params()

            # Initialize Cosy TTS client
            self.client = CosyTTSClient(self.config, ten_env, self.vendor())
            self.audio_processor_task = asyncio.create_task(
                self._process_audio_data()
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self._send_tts_error(str(e))

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        await super().on_start(ten_env)
        ten_env.log_info("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.audio_processor_task:
            self.audio_processor_task.cancel()
            try:
                await self.audio_processor_task
            except asyncio.CancelledError:
                ten_env.log_info("Audio processor task cancelled.")
            self.audio_processor_task = None

        if self.client:
            # The new client is stateless, no stop method needed.
            self.client = None

        # Clean up all PCMWriters
        await self._cleanup_all_pcm_writers()

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def on_data(self, ten_env: AsyncTenEnv, data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data: {data_name}")

        if data.get_name() == DATA_FLUSH:
            # Flush the current request
            ten_env.log_info(
                f"Received flush request, current_request_id: {self.current_request_id}"
            )
            await self._flush()
            self.current_request_finished = True
            if self.request_start_ts:
                await self._handle_tts_audio_end(TTSAudioEndReason.INTERRUPTED)

        await super().on_data(ten_env, data)

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end}, request_id: {t.request_id}, current_request_id: {self.current_request_id}"
            )

            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )
                self.client.synthesizer = None
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0  # Reset for new request
                self.request_ttfb = None

                if t.metadata is not None:
                    self.current_turn_id = t.metadata.get("turn_id", -1)

                # Manage PCMWriter instances for audio recording
                await self._manage_pcm_writers(t.request_id)

            elif self.current_request_finished:
                error_msg = f"Received a message for a finished request_id '{t.request_id}' with text_input_end=False."
                self.ten_env.log_error(error_msg)
                return

            # Check if text is empty
            if t.text.strip() == "":
                self.ten_env.log_info(
                    f"Received empty text for TTS request, text_input_end: {t.text_input_end}"
                )
                if t.text_input_end and not self.current_request_finished:
                    await self.client.complete()
                    self.current_request_finished = True
                    await self._handle_tts_audio_end()
                    return

            # Record TTFB timing
            if self.request_start_ts is None:
                self.request_start_ts = datetime.now()

            # Get audio stream from Cosy TTS
            self.ten_env.log_info(
                f"Calling client.synthesize_audio() with text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )

            # Start audio synthesis (returns immediately)
            self.client.synthesize_audio(t.text, t.text_input_end)

            # Handle text input end
            if t.text_input_end:
                self.ten_env.log_info(
                    f"KEYPOINT finish session for request ID: {t.request_id}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                )
                self.current_request_finished = True

        except WebSocketConnectionClosedException as e:
            self.ten_env.log_error(f"WebSocket connection closed, {e}")
            await self._send_tts_error(
                str(e),
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )
            self.client.synthesizer = None

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            await self._send_tts_error(
                str(e),
                code=ModuleErrorCode.FATAL_ERROR.value,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )
            self.client.synthesizer = None

    async def _process_audio_data(self) -> None:
        """
        Independent audio data process loop.
        This runs in the background and processes audio data from the client.
        """
        chunk_count = 0
        first_chunk = True

        try:
            self.ten_env.log_info("Starting audio process loop")

            while True:  # Loop until we get a done signal or error
                try:
                    self.ten_env.log_info(
                        "Waiting for audio data from client..."
                    )
                    # Get audio data from client
                    done, message_type, data = (
                        await self.client.get_audio_data()
                    )

                    self.ten_env.log_info(
                        f"Received done: {done}, message_type: {message_type}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                    )

                    # Process PCM audio chunks
                    if message_type == MESSAGE_TYPE_PCM:
                        audio_chunk = data

                        if (
                            audio_chunk is not None
                            and len(audio_chunk) > 0
                            and isinstance(audio_chunk, bytes)
                        ):
                            chunk_count += 1
                            self.total_audio_bytes += len(audio_chunk)
                            self.ten_env.log_info(
                                f"[tts] Received audio chunk #{chunk_count}, size: {len(audio_chunk)} bytes, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                            )

                            # Send TTS audio start on first chunk
                            if first_chunk:
                                await self._handle_first_audio_chunk()
                                first_chunk = False

                            # Write to dump file if enabled
                            await self._write_audio_to_dump_file(audio_chunk)

                            # Send audio data
                            await self.send_tts_audio_data(audio_chunk)
                        else:
                            self.ten_env.log_info(
                                f"Received empty or invalid payload for TTS response, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                            )
                    elif message_type == MESSAGE_TYPE_CMD_ERROR:
                        self.ten_env.log_error(
                            f"Received error message from client: {data}"
                        )
                        await self._send_tts_error(
                            str(data),
                            code=ModuleErrorCode.NON_FATAL_ERROR.value,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        )

                    elif message_type == MESSAGE_TYPE_CMD_CANCEL:
                        self.ten_env.log_info(
                            f"Received cancel message from client: {data}"
                        )

                    # Handle TTS audio end - this is when we should stop
                    if done:
                        self.ten_env.log_info(
                            f"All pcm received done, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                        )
                        await self._handle_tts_audio_end()

                except asyncio.CancelledError:
                    self.ten_env.log_info("Audio consumer task was cancelled.")
                    break
                except Exception as e:
                    self.ten_env.log_error(f"Error in audio consumer loop: {e}")
                    self.ten_env.log_error(
                        "Audio consumer loop breaking due to exception"
                    )
                    # Send an error message to notify the system of the failure
                    await self._send_tts_error(
                        str(e),
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                    )

            self.ten_env.log_info(
                f"Audio consumer completed, total chunks: {chunk_count}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            # Reset for next request
            self.request_start_ts = None

        except Exception as e:
            self.ten_env.log_error(f"Fatal error in audio consumer: {e}")
            await self._send_tts_error(
                str(e),
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )

    def synthesize_audio_sample_rate(self) -> int:
        """
        Get the sample rate for the TTS audio.
        """
        return self.config.sample_rate

    def vendor(self) -> str:
        """
        Get the vendor name for the TTS audio.
        """
        return "cosy"

    def _calculate_ttfb_ms(self, start_time: datetime) -> int:
        """
        Calculate Time To First Byte (TTFB) in milliseconds.

        Args:
            start_time: The timestamp when the request was sent

        Returns:
            TTFB in milliseconds
        """
        return int((datetime.now() - start_time).total_seconds() * 1000)

    def _calculate_audio_duration(
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

    async def _cleanup_all_pcm_writers(self) -> None:
        """
        Clean up all PCMWriter instances.
        This is typically called during shutdown or cleanup operations.
        """
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                self.ten_env.log_info(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                self.ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        # Clear the recorder map
        self.recorder_map.clear()

    async def _flush(self) -> None:
        """
        Flush the TTS request.
        """
        if self.client:
            self.ten_env.log_info(
                f"Flushing TTS for request ID: {self.current_request_id}"
            )
            self.client.cancel()

    def _get_pcm_dump_file_path(self, request_id: str) -> str:
        """
        Get the PCM dump file path.

        Returns:
            str: The complete path of the PCM dump file
        """
        if self.config is None:
            raise ValueError(
                "Configuration not initialized, cannot get PCM dump file path"
            )

        return os.path.join(
            self.config.dump_path,
            generate_file_name(f"{self.name}_out_{request_id}"),
        )

    async def _handle_first_audio_chunk(self) -> None:
        """
        Handle the first audio chunk from TTS service.

        This method:
        1. Sends TTS audio start event
        2. Calculates and records TTFB (Time To First Byte)
        3. Sends TTFB metrics
        4. Logs the operation
        """
        if self.request_start_ts:
            await self.send_tts_audio_start(
                self.current_request_id,
                self.current_turn_id,
            )

            self.request_ttfb = self._calculate_ttfb_ms(self.request_start_ts)
            await self.send_tts_ttfb_metrics(
                self.current_request_id,
                self.request_ttfb,
                self.current_turn_id,
            )

            self.ten_env.log_info(
                f"KEYPOINT Sent TTS audio start and TTFB metrics: {self.request_ttfb}ms, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )

    async def _handle_tts_audio_end(
        self,
        reason: TTSAudioEndReason = TTSAudioEndReason.REQUEST_END,
    ) -> None:
        """
        Handle TTS audio end processing.

        This method:
        1. Calculates total audio duration
        2. Calculates request event interval
        3. Sends TTS audio end event
        4. Logs the operation
        """
        if self.request_start_ts:
            self.request_total_audio_duration_ms = (
                self._calculate_audio_duration(
                    self.total_audio_bytes, self.config.sample_rate
                )
            )
            request_event_interval = int(
                (datetime.now() - self.request_start_ts).total_seconds() * 1000
            )

            # Send TTS audio end event
            await self.send_tts_audio_end(
                self.current_request_id,
                request_event_interval,
                self.request_total_audio_duration_ms,
                self.current_turn_id,
                reason,
            )

            self.ten_env.log_info(
                f"KEYPOINT Sent TTS audio end event, interval: {request_event_interval}ms, duration: {self.request_total_audio_duration_ms}ms, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )

    async def _manage_pcm_writers(self, request_id: str) -> None:
        """
        Manage PCMWriter instances for audio recording.
        Creates new PCMWriter for current request and cleans up old ones.

        Args:
            request_id: Current request ID to keep active
        """
        if not self.config or not self.config.dump:
            return

        # Clean up old PCMWriters (except current request_id)
        old_request_ids = [
            rid for rid in self.recorder_map.keys() if rid != request_id
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

        # Create new PCMWriter if needed
        if request_id not in self.recorder_map:
            dump_file_path = self._get_pcm_dump_file_path(request_id)
            self.recorder_map[request_id] = PCMWriter(dump_file_path)
            self.ten_env.log_info(
                f"Created PCMWriter for request_id: {request_id}, file: {dump_file_path}"
            )

    async def _send_tts_error(
        self,
        message: str,
        vendor_code: str | None = None,
        vendor_message: str | None = None,
        vendor_info: ModuleErrorVendorInfo | None = None,
        code: int = ModuleErrorCode.FATAL_ERROR.value,
        request_id: str | None = None,
    ) -> None:
        """
        Send a TTS error message.
        """
        if vendor_code is not None:
            vendor_info = ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=vendor_code,
                message=vendor_message or "",
            )

        await self.send_tts_error(
            request_id or self.current_request_id,
            ModuleError(
                message=message,
                module=ModuleType.TTS,
                code=code,
                vendor_info=vendor_info,
            ),
        )

    async def _write_audio_to_dump_file(self, audio_chunk: bytes) -> None:
        """
        Write audio chunk to dump file if enabled.
        """
        if (
            self.config
            and self.config.dump
            and self.current_request_id
            and self.current_request_id in self.recorder_map
        ):
            self.ten_env.log_info(
                f"KEYPOINT Writing audio chunk to dump file, dump path: {self.config.dump_path}, request_id: {self.current_request_id}"
            )
            asyncio.create_task(
                self.recorder_map[self.current_request_id].write(audio_chunk)
            )
