#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

import asyncio
import numpy as np
from typing import Optional, Callable
from funasr import AutoModel
from funasr.utils.postprocess_utils import rich_transcription_postprocess


class FunASRClient:
    """Client for local FunASR ASR processing (SenseVoice / Fun-ASR-Nano / Paraformer)."""

    def __init__(
        self,
        model: str = "iic/SenseVoiceSmall",
        device: str = "cpu",
        language: str = "auto",
        use_itn: bool = True,
        sample_rate: int = 16000,
        on_result_callback: Optional[Callable] = None,
        on_error_callback: Optional[Callable] = None,
        logger: Optional[any] = None,
    ):
        self.model_id = model
        self.device = device
        self.language = language
        self.use_itn = use_itn
        self.sample_rate = sample_rate
        self.on_result_callback = on_result_callback
        self.on_error_callback = on_error_callback
        self.logger = logger

        self.model: Optional[AutoModel] = None
        self.audio_buffer = bytearray()
        self.is_connected_flag = False
        self.processing_lock = asyncio.Lock()

        # Buffer settings
        self.min_audio_length_ms = 1000  # Minimum 1 second of audio
        self.max_audio_length_ms = 30000  # Maximum 30 seconds per chunk

    async def connect(self) -> None:
        """Load the local FunASR model (off the event loop)."""
        try:
            if self.logger:
                self.logger.log_info(
                    f"Loading FunASR model: {self.model_id} on {self.device}"
                )

            loop = asyncio.get_event_loop()
            self.model = await loop.run_in_executor(
                None,
                lambda: AutoModel(
                    model=self.model_id,
                    device=self.device,
                    disable_update=True,
                ),
            )

            self.is_connected_flag = True
            if self.logger:
                self.logger.log_info("FunASR model loaded successfully")

        except Exception as e:
            self.is_connected_flag = False
            if self.logger:
                self.logger.log_error(f"Failed to load FunASR model: {e}")
            if self.on_error_callback:
                await self.on_error_callback(str(e))
            raise

    async def disconnect(self) -> None:
        """Clean up resources"""
        self.is_connected_flag = False
        self.model = None
        self.audio_buffer.clear()
        if self.logger:
            self.logger.log_info("FunASR client disconnected")

    def is_connected(self) -> bool:
        """Check if model is loaded"""
        return self.is_connected_flag and self.model is not None

    async def send_audio(self, audio_data: bytes) -> None:
        """Add audio data to buffer and process once enough has accumulated."""
        if not self.is_connected():
            return

        self.audio_buffer.extend(audio_data)

        # Calculate buffer duration in milliseconds (16-bit PCM => 2 bytes/sample)
        buffer_duration_ms = (
            len(self.audio_buffer) / (self.sample_rate * 2) * 1000
        )

        if buffer_duration_ms >= self.min_audio_length_ms:
            await self._process_audio()

    async def _process_audio(self) -> None:
        """Process accumulated audio buffer with the FunASR model."""
        async with self.processing_lock:
            if len(self.audio_buffer) == 0:
                return

            try:
                # 16-bit PCM bytes -> float32 in [-1, 1]
                audio_np = (
                    np.frombuffer(self.audio_buffer, dtype=np.int16).astype(
                        np.float32
                    )
                    / 32768.0
                )

                # Limit to max length
                max_samples = int(
                    self.max_audio_length_ms / 1000 * self.sample_rate
                )
                if len(audio_np) > max_samples:
                    audio_np = audio_np[:max_samples]

                duration_ms = int(len(audio_np) / self.sample_rate * 1000)

                # Run inference in a thread pool to avoid blocking the loop
                loop = asyncio.get_event_loop()
                res = await loop.run_in_executor(
                    None,
                    lambda: self.model.generate(
                        input=audio_np,
                        language=self.language,
                        use_itn=self.use_itn,
                    ),
                )

                # SenseVoice output carries tags like <|zh|><|NEUTRAL|>...; strip them.
                text = (
                    rich_transcription_postprocess(res[0]["text"]).strip()
                    if res
                    else ""
                )

                if text and self.on_result_callback:
                    await self.on_result_callback(
                        text=text,
                        start_ms=0,
                        duration_ms=duration_ms,
                        language=self.language,
                        final=True,
                    )

                # Clear processed audio
                self.audio_buffer.clear()

            except Exception as e:
                # Clear buffer on error to prevent accumulation
                self.audio_buffer.clear()
                if self.logger:
                    self.logger.log_error(f"Error processing audio: {e}")
                if self.on_error_callback:
                    await self.on_error_callback(str(e))

    async def finalize(self) -> None:
        """Process any remaining audio in buffer"""
        if len(self.audio_buffer) > 0:
            await self._process_audio()
