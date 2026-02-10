#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

import asyncio
import numpy as np
from typing import Optional, Callable
from faster_whisper import WhisperModel


class WhisperClient:
    """Client for faster-whisper ASR processing"""

    def __init__(
        self,
        model_size: str = "base",
        device: str = "cpu",
        compute_type: str = "int8",
        language: Optional[str] = None,
        task: str = "transcribe",
        sample_rate: int = 16000,
        on_result_callback: Optional[Callable] = None,
        on_error_callback: Optional[Callable] = None,
        logger: Optional[any] = None,
    ):
        self.model_size = model_size
        self.device = device
        self.compute_type = compute_type
        self.language = language
        self.task = task
        self.sample_rate = sample_rate
        self.on_result_callback = on_result_callback
        self.on_error_callback = on_error_callback
        self.logger = logger

        self.model: Optional[WhisperModel] = None
        self.audio_buffer = bytearray()
        self.is_connected_flag = False
        self.processing_lock = asyncio.Lock()

        # Buffer settings
        self.min_audio_length_ms = 1000  # Minimum 1 second of audio
        self.max_audio_length_ms = 30000  # Maximum 30 seconds (Whisper limit)

    async def connect(self) -> None:
        """Initialize the Whisper model"""
        try:
            if self.logger:
                self.logger.log_info(
                    f"Loading Whisper model: {self.model_size} on {self.device}"
                )

            # Load model in a thread pool to avoid blocking
            loop = asyncio.get_event_loop()
            self.model = await loop.run_in_executor(
                None,
                lambda: WhisperModel(
                    self.model_size,
                    device=self.device,
                    compute_type=self.compute_type,
                ),
            )

            self.is_connected_flag = True
            if self.logger:
                self.logger.log_info("Whisper model loaded successfully")

        except Exception as e:
            self.is_connected_flag = False
            if self.logger:
                self.logger.log_error(f"Failed to load Whisper model: {e}")
            if self.on_error_callback:
                await self.on_error_callback(str(e))
            raise

    async def disconnect(self) -> None:
        """Clean up resources"""
        self.is_connected_flag = False
        self.model = None
        self.audio_buffer.clear()
        if self.logger:
            self.logger.log_info("Whisper client disconnected")

    def is_connected(self) -> bool:
        """Check if model is loaded"""
        return self.is_connected_flag and self.model is not None

    async def send_audio(self, audio_data: bytes) -> None:
        """Add audio data to buffer"""
        if not self.is_connected():
            return

        self.audio_buffer.extend(audio_data)

        # Calculate buffer duration in milliseconds
        buffer_duration_ms = (
            len(self.audio_buffer) / (self.sample_rate * 2) * 1000
        )

        # Process if we have enough audio (but not too much)
        if buffer_duration_ms >= self.min_audio_length_ms:
            await self._process_audio()

    async def _process_audio(self) -> None:
        """Process accumulated audio buffer"""
        async with self.processing_lock:
            if len(self.audio_buffer) == 0:
                return

            try:
                # Convert bytes to numpy array
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

                # Run transcription in thread pool
                loop = asyncio.get_event_loop()
                segments, info = await loop.run_in_executor(
                    None,
                    lambda: self.model.transcribe(
                        audio_np,
                        language=self.language,
                        task=self.task,
                        beam_size=5,
                        vad_filter=True,
                        vad_parameters=dict(
                            min_silence_duration_ms=500,
                            speech_pad_ms=400,
                        ),
                    ),
                )

                # Process segments
                for segment in segments:
                    if self.on_result_callback:
                        await self.on_result_callback(
                            text=segment.text.strip(),
                            start_ms=int(segment.start * 1000),
                            duration_ms=int(
                                (segment.end - segment.start) * 1000
                            ),
                            language=info.language if info else self.language,
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
