#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback

from ten_ai_base.helper import generate_file_name, PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    ModuleVendorException,
)
from ten_ai_base.struct import TTSFlush, TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension, DATA_FLUSH
from ten_runtime import AsyncTenEnv

from .config import TencentTTSConfig
from .tencent_tts import (
    MESSAGE_TYPE_PCM,
    TencentTTSClient,
    TencentTTSTaskFailedException,
)


class TencentTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

        # TTS client for Tencent TTS service
        self.client: TencentTTSClient | None = None
        # Configuration for TTS settings
        self.config: TencentTTSConfig | None = None
        # Flag indicating if current request is finished
        self.current_request_finished: bool = False
        # ID of the current TTS request being processed
        self.current_request_id: str | None = None
        # Turn ID for conversation tracking
        self.current_turn_id: int = -1
        # Extension name for logging and identification
        self.name: str = name
        # Path to PCM dump file for audio recording
        self.pcm_dump_file: str | None = None
        # PCM writer for dumping audio data to file
        self.recorder: PCMWriter | None = None
        # Timestamp when TTS request was sent to service
        self.request_start_ts: datetime | None = None
        # Total audio duration for current request in milliseconds
        self.request_total_audio_duration_ms: int | None = None
        # Time to first byte for current request in milliseconds
        self.request_ttfb: int | None = None
        # Session ID for conversation context
        self.session_id: str = ""
        # Total audio bytes received for current request
        self.total_audio_bytes: int = 0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = TencentTTSConfig.model_validate_json(config_json)
                # Update params from config
                self.config.update_params()

                self.ten_env.log_info(
                    f"KEYPOINT config: {self.config.to_str()}"
                )

                # Validate params
                self.config.validate_params()

            # Setup PCM dump file
            self.pcm_dump_file = self._get_pcm_dump_file_path()
            self.recorder = PCMWriter(self.pcm_dump_file)

            # Initialize Tencent TTS client
            self.client = TencentTTSClient(self.config, ten_env, self.vendor())
            asyncio.create_task(self.client.start())
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self._send_tts_error(str(e))

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        await super().on_start(ten_env)
        ten_env.log_info("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            await self.client.close()

        if self.recorder:
            await self.recorder.flush()

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def on_data(self, ten_env: AsyncTenEnv, data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data: {data_name}")

        if data.get_name() == DATA_FLUSH:
            await self._flush()

        await super().on_data(ten_env, data)

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
            )

            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )

                self.current_request_id = t.request_id
                self.request_total_audio_duration = 0
                self.request_ttfb = None

                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)
            elif self.current_request_finished:
                if not t.text_input_end:
                    error_msg = f"Received a message for a finished request_id '{t.request_id}' with text_input_end=False."
                    self.ten_env.log_error(error_msg)
                    await self._send_tts_error(
                        error_msg,
                        vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        request_id=t.request_id,
                    )
                return

            # Check if text is empty
            if t.text.strip() == "":
                self.ten_env.log_info("Received empty text for TTS request")
                if t.text_input_end:
                    self.current_request_finished = True
                return

            # Record TTFB timing
            if self.request_start_ts is None:
                self.request_start_ts = datetime.now()

            # Get audio stream from Tencent TTS
            self.ten_env.log_info(
                f"Calling client.synthesize_audio() with text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            data = self.client.synthesize_audio(t.text)
            self.ten_env.log_info(f"Got data generator: {data}")

            # Process audio chunks
            chunk_count = 0
            first_chunk = True

            async for [done, message_type, message] in data:
                self.ten_env.log_info(
                    f"Received done: {done}, message_type: {message_type}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                )

                # Process PCM audio chunks
                if message_type == MESSAGE_TYPE_PCM:
                    audio_chunk = message

                    if audio_chunk is not None and len(audio_chunk) > 0:
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
                        self.ten_env.log_error(
                            f"Received empty payload for TTS response, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                        )

                # Handle TTS audio end
                if done:
                    self.ten_env.log_info(
                        f"All pcm received done, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                    )
                    await self._handle_tts_audio_end()
                    break

            self.ten_env.log_info(
                f"TTS processing completed, total chunks: {chunk_count}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            # Reset for next request
            self.request_start_ts = None

            # Handle text input end
            if t.text_input_end:
                self.ten_env.log_info(
                    f"KEYPOINT finish session for request ID: {t.request_id}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
                )
                self.current_request_finished = True

        except TencentTTSTaskFailedException as e:
            self.ten_env.log_error(
                f"TencentTTSTaskFailedException in request_tts: {e.error_msg} (code: {e.error_code}). text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            await self._send_tts_error(
                e.error_msg,
                str(e.error_code),
                e.error_msg,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
            )

        except ModuleVendorException as e:
            self.ten_env.log_error(
                f"ModuleVendorException in request_tts: {traceback.format_exc()}. text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
            await self._send_tts_error(
                str(e),
                e.error.code,
                e.error.message,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
            )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
            )
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
        return "tencent"

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

    async def _flush(self) -> None:
        """
        Flush the TTS request.
        """
        if self.client:
            self.ten_env.log_info(
                f"Flushing TTS for request ID: {self.current_request_id}"
            )
            await self.client.cancel()

    def _get_pcm_dump_file_path(self) -> str:
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
            self.config.dump_path, generate_file_name(f"{self.name}_out")
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

    async def _handle_tts_audio_end(self) -> None:
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

            await self.send_tts_audio_end(
                self.current_request_id,
                request_event_interval,
                self.request_total_audio_duration_ms,
                self.current_turn_id,
            )

            self.ten_env.log_info(
                f"KEYPOINT Sent TTS audio end event, interval: {request_event_interval}ms, duration: {self.request_total_audio_duration_ms}ms, current_request_id: {self.current_request_id}, current_turn_id: {self.current_turn_id}"
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
                module="tts",  # Use string directly instead of enum
                code=code,
                vendor_info=vendor_info,
            ),
        )

    async def _write_audio_to_dump_file(self, audio_chunk: bytes) -> None:
        """
        Write audio chunk to dump file if enabled.
        """
        if self.config and self.config.dump and self.recorder:
            self.ten_env.log_info(
                f"KEYPOINT Writing audio chunk to dump file, dump file: {self.pcm_dump_file}"
            )
            asyncio.create_task(self.recorder.write(audio_chunk))
