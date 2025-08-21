import asyncio
from collections.abc import AsyncIterator
from datetime import datetime

import dashscope
from dashscope.audio.tts_v2 import (
    SpeechSynthesizer,
    AudioFormat,
    ResultCallback,
)

from .config import CosyTTSConfig
from ten_runtime.async_ten_env import AsyncTenEnv


MESSAGE_TYPE_PCM = 1
MESSAGE_TYPE_CMD_COMPLETE = 2
MESSAGE_TYPE_CMD_ERROR = 3

ERROR_CODE_TTS_FAILED = -1

# Audio format mapping constants
AUDIO_FORMAT_MAPPING = {
    8000: AudioFormat.PCM_8000HZ_MONO_16BIT,
    16000: AudioFormat.PCM_16000HZ_MONO_16BIT,
    22050: AudioFormat.PCM_22050HZ_MONO_16BIT,
    24000: AudioFormat.PCM_24000HZ_MONO_16BIT,
    44100: AudioFormat.PCM_44100HZ_MONO_16BIT,
    48000: AudioFormat.PCM_48000HZ_MONO_16BIT,
}
DEFAULT_AUDIO_FORMAT = AudioFormat.PCM_16000HZ_MONO_16BIT


class CosyTTSTaskFailedException(Exception):
    """Exception raised when Cosy TTS task fails"""

    error_code: int
    error_msg: str

    def __init__(self, error_code: int, error_msg: str):
        self.error_code = error_code
        self.error_msg = error_msg
        super().__init__(f"TTS task failed: {error_msg} (code: {error_code})")


class AsyncIteratorCallback(ResultCallback):
    """Callback class for handling TTS synthesis results asynchronously."""

    def __init__(
        self,
        ten_env: AsyncTenEnv,
        queue: asyncio.Queue[tuple[bool, int, str | bytes | None]],
    ) -> None:
        self.ten_env = ten_env

        self._closed = False
        self._loop = asyncio.get_event_loop()
        self._queue = queue

    def close(self):
        """Close the callback."""
        self._closed = True

    def on_open(self):
        """Called when WebSocket connection opens."""
        self.ten_env.log_info("WebSocket connection opened for TTS synthesis.")

    def on_complete(self):
        """Called when TTS synthesis completes successfully."""
        self.ten_env.log_info("TTS synthesis task completed successfully.")

        # Send completion signal
        asyncio.run_coroutine_threadsafe(
            self._queue.put((True, MESSAGE_TYPE_CMD_COMPLETE, None)), self._loop
        )

    def on_error(self, message: str):
        """Called when TTS synthesis encounters an error."""
        self.ten_env.log_error(f"TTS synthesis task failed: {message}")

        # Send error signal
        asyncio.run_coroutine_threadsafe(
            self._queue.put((True, MESSAGE_TYPE_CMD_ERROR, message)), self._loop
        )

    def on_close(self):
        """Called when WebSocket connection closes."""
        self.ten_env.log_info("WebSocket connection closed.")
        self.close()

    def on_event(self, message: str) -> None:
        """Called when receiving events from TTS service."""
        self.ten_env.log_debug(f"Received TTS event: {message}")

    def on_data(self, data: bytes) -> None:
        """Called when receiving audio data from TTS service."""
        if self._closed:
            self.ten_env.log_warn(
                f"Received {len(data)} bytes but connection was closed"
            )
            return

        self.ten_env.log_debug(f"Received audio data: {len(data)} bytes")
        # Send audio data to queue
        asyncio.run_coroutine_threadsafe(
            self._queue.put((False, MESSAGE_TYPE_PCM, data)), self._loop
        )


class CosyTTSClient:
    """Client for Cosy TTS service using dashscope."""

    def __init__(
        self,
        config: CosyTTSConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
    ):
        # Configuration and environment
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor

        # Session management
        self.stopping: bool = False
        self.turn_id: int = 0

        # TTS synthesizer
        self._callback: AsyncIteratorCallback | None = None
        self.synthesizer: SpeechSynthesizer | None = None

        # Communication queue for audio data
        self._receive_queue: (
            asyncio.Queue[tuple[bool, int, str | bytes | None]] | None
        ) = None

        # Set dashscope API key
        dashscope.api_key = config.api_key

    async def start(self) -> None:
        """Start the TTS client and initialize components."""
        # Initialize audio data queue
        self._receive_queue = asyncio.Queue()
        self._callback = AsyncIteratorCallback(
            self.ten_env, self._receive_queue
        )

        # Create synthesizer with configuration
        self.synthesizer = SpeechSynthesizer(
            callback=self._callback,
            format=self._get_audio_format(),
            model=self.config.model,
            voice=self.config.voice,
        )

        # Pre-connection to ensure service is accessible
        self.ten_env.log_info("Pre-connection TTS service connection...")
        # Start a test synthesis
        self.synthesizer.streaming_call("")
        self.ten_env.log_info("Cosy TTS client started successfully")

    async def cancel(self) -> None:
        """
        Cancel current TTS operation.
        """
        if self.synthesizer:
            try:
                self.synthesizer.streaming_cancel()
                self.ten_env.log_info("TTS operation cancelled")
            except Exception as e:
                self.ten_env.log_error(f"Error cancelling TTS: {e}")

            # Clean up synthesizer
            self.synthesizer = None

    async def stop(self) -> None:
        """
        Close the TTS client and cleanup resources.
        """
        self.stopping = True
        # Cancel any ongoing synthesis
        await self.cancel()
        self.ten_env.log_info(
            f"Cosy TTS client closed successfully, stopping: {self.stopping}"
        )

    async def complete(self) -> None:
        """
        Complete current TTS operation.
        """
        if self.synthesizer:
            try:
                self.synthesizer.streaming_complete()
                self.ten_env.log_info("TTS operation completed")
            except Exception as e:
                self.ten_env.log_error(f"Error completing TTS: {e}")

            # Clean up synthesizer
            self.synthesizer = None

    async def synthesize_audio(
        self, text: str, text_input_end: bool
    ) -> AsyncIterator[tuple[bool, int, str | bytes | None]]:
        """Convert text to speech audio stream using Cosy TTS service."""
        start_time = datetime.now()

        try:
            self.ten_env.log_info(f"Starting TTS synthesis, text: {text}")

            # Start synthesizer if not initialized
            if self.synthesizer is None:
                await self.start()

            # Start streaming TTS synthesis
            self.synthesizer.streaming_call(text)

            # Complete streaming
            if text_input_end:
                await self.complete()

            # Process audio chunks from queue
            while not self.stopping:
                try:
                    if self._receive_queue is None:
                        self.ten_env.log_error(
                            "TTS receive queue is not initialized"
                        )
                        break

                    done, message_type, data = await asyncio.wait_for(
                        self._receive_queue.get(), timeout=5
                    )

                    # Yield the data
                    yield (done, message_type, data)

                    # If done, break the loop
                    if done:
                        self.ten_env.log_info(
                            f"TTS synthesis completed: duration={self._duration_in_ms_since(start_time)}ms"
                        )
                        break

                except asyncio.TimeoutError:
                    self.ten_env.log_warn(
                        f"Timeout waiting for TTS audio data, stopping: {self.stopping}"
                    )
                    # Force exit the loop when timeout occurs to prevent infinite loop
                    break

            self.ten_env.log_info(
                f"TTS synthesis completed: duration={self._duration_in_ms_since(start_time)}ms"
            )

        except Exception as e:
            self.ten_env.log_error(f"TTS synthesis failed: {e}")
            raise CosyTTSTaskFailedException(
                error_code=ERROR_CODE_TTS_FAILED,
                error_msg=str(e),
            ) from e

    def _duration_in_ms(self, start: datetime, end: datetime) -> int:
        """
        Calculate duration between two timestamps in milliseconds.

        Args:
            start: Start timestamp
            end: End timestamp

        Returns:
            Duration in milliseconds
        """
        return int((end - start).total_seconds() * 1000)

    def _duration_in_ms_since(self, start: datetime) -> int:
        """
        Calculate duration from a timestamp to now in milliseconds.

        Args:
            start: Start timestamp

        Returns:
            Duration in milliseconds from start to now
        """
        return self._duration_in_ms(start, datetime.now())

    def _get_audio_format(self) -> AudioFormat:
        """
        Automatically generate AudioFormat based on configuration.

        Returns:
            AudioFormat: The appropriate audio format for the configuration
        """
        if self.config.sample_rate in AUDIO_FORMAT_MAPPING:
            return AUDIO_FORMAT_MAPPING[self.config.sample_rate]

        # Fallback to default format if configuration not supported
        self.ten_env.log_warn(
            f"Unsupported audio format: {self.config.sample_rate}Hz, using default format: PCM_16000HZ_MONO_16BIT"
        )
        return DEFAULT_AUDIO_FORMAT
