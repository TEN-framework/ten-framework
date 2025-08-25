import asyncio
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
MESSAGE_TYPE_CMD_CANCEL = 4

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

        self.ten_env.log_info(f"Received audio data: {len(data)} bytes")
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

        # Clean up callback and queue state to prevent stale data issues
        if self._callback:
            self._callback.close()
            self._callback = None

        # Clear the queue to prevent stale data and signal consumer to stop
        if self._receive_queue:
            # Clear any remaining items in the queue
            while not self._receive_queue.empty():
                try:
                    self._receive_queue.get_nowait()
                except asyncio.QueueEmpty:
                    break
            # Signal the consumer that this session is cancelled
            await self._receive_queue.put(
                (True, MESSAGE_TYPE_CMD_CANCEL, "Operation cancelled")
            )

        self.ten_env.log_info("TTS client state cleaned up after cancel")

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
                self.synthesizer.async_streaming_complete()
                self.ten_env.log_info("TTS operation completed")
            except Exception as e:
                self.ten_env.log_error(f"Error completing TTS: {e}")

            # Clean up synthesizer
            self.synthesizer = None

    async def synthesize_audio(self, text: str, text_input_end: bool):
        """
        Start audio synthesis for the given text.
        This method only initiates synthesis and returns immediately.
        Audio data should be consumed from the queue independently.
        """
        self.ten_env.log_info(
            f"Starting TTS synthesis, text: {text}, input_end: {text_input_end}"
        )

        # Start synthesizer if not initialized
        if self.synthesizer is None:
            self.ten_env.log_info(
                "Synthesizer is not initialized, starting new one."
            )
            await self.start()

        # Start streaming TTS synthesis
        assert self.synthesizer is not None
        self.synthesizer.streaming_call(text)

        # Complete streaming if this is the end
        if text_input_end:
            await self.complete()

        self.ten_env.log_info(f"TTS synthesis initiated for text: {text}")

    async def get_audio_data(self):
        """
        Get audio data from the queue. This is a separate method that can be called
        independently to consume audio data.
        Returns: (done, message_type, data) or None if queue is not initialized
        """
        if self._receive_queue is None:
            return None
        return await self._receive_queue.get()

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
