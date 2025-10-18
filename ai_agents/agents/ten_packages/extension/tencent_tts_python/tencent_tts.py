import asyncio
from datetime import datetime


from .config import TencentTTSConfig
from ten_runtime.async_ten_env import AsyncTenEnv
from .src.flowing_speech_synthesizer import (
    FlowingSpeechSynthesizer,
    FlowingSpeechSynthesisListener,
)
from .src.common import credential

MESSAGE_TYPE_PCM = 1
MESSAGE_TYPE_CMD_COMPLETE = 2
MESSAGE_TYPE_CMD_ERROR = 3
MESSAGE_TYPE_CMD_METRIC = 4


class TencentTTSTaskFailedException(Exception):
    def __init__(self, error_code: int, error_message: str):
        self.error_code = error_code
        self.error_message = error_message

    def __str__(self):
        return f"TencentTTSTaskFailedException: {self.error_code}, {self.error_message}"


class AsyncIteratorCallback(FlowingSpeechSynthesisListener):
    """Callback class for handling TTS synthesis results asynchronously."""

    def __init__(
        self,
        ten_env: AsyncTenEnv,
        queue: asyncio.Queue[
            tuple[
                bool,
                int,
                str | bytes | TencentTTSTaskFailedException | int | None,
            ]
        ],
    ) -> None:
        self.ten_env = ten_env

        self._loop = asyncio.get_event_loop()
        self._queue = queue
        self.sent_ts: datetime | None = None
        self.ttfb_sent: bool = False

    def set_sent_ts(self):
        if self.sent_ts:
            return
        self.sent_ts = datetime.now()

    def on_close(self):
        super().on_close()
        self.ten_env.log_debug("WebSocket connection closed.")

    def on_synthesis_start(self, session_id) -> None:
        super().on_synthesis_start(session_id)
        self.ten_env.log_debug(
            f"TTS synthesis task started, session_id: {session_id}"
        )

    def on_synthesis_end(self) -> None:
        super().on_synthesis_end()
        self.ten_env.log_debug("TTS synthesis task completed")
        asyncio.run_coroutine_threadsafe(
            self._queue.put((True, MESSAGE_TYPE_CMD_COMPLETE, None)), self._loop
        )

    def on_audio_result(self, audio_bytes):
        super().on_audio_result(audio_bytes)
        if self.sent_ts and not self.ttfb_sent:
            ttfb_ms = int(
                (datetime.now() - self.sent_ts).total_seconds() * 1000
            )
            self.ttfb_sent = True
            asyncio.run_coroutine_threadsafe(
                self._queue.put((False, MESSAGE_TYPE_CMD_METRIC, ttfb_ms)),
                self._loop,
            )
        self.ten_env.log_debug(f"Received audio data: {len(audio_bytes)} bytes")
        # Send audio data to queue
        asyncio.run_coroutine_threadsafe(
            self._queue.put((False, MESSAGE_TYPE_PCM, audio_bytes)), self._loop
        )

    def on_synthesis_fail(self, response):
        super().on_synthesis_fail(response)

        err_code = response["code"]
        message = response["message"]
        self.ten_env.log_error(
            f"TTS synthesis task failed: {err_code}, {message}"
        )

        # Send error signal
        asyncio.run_coroutine_threadsafe(
            self._queue.put(
                (
                    True,
                    MESSAGE_TYPE_CMD_ERROR,
                    TencentTTSTaskFailedException(err_code, message),
                )
            ),
            self._loop,
        )

    def on_data(self, data: bytes) -> None:
        """Called when receiving audio data from TTS service."""
        self.ten_env.log_debug(f"Received audio data: {len(data)} bytes")
        # Send audio data to queue
        asyncio.run_coroutine_threadsafe(
            self._queue.put((False, MESSAGE_TYPE_PCM, data)), self._loop
        )


class TencentTTSClient:
    """Client for Tencent TTS service."""

    def __init__(
        self,
        config: TencentTTSConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
    ):
        # Configuration and environment
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.conn_ready_event = asyncio.Event()

        # TTS synthesizer
        self._callback: AsyncIteratorCallback | None = None
        self.synthesizer: FlowingSpeechSynthesizer | None = None
        # Communication queue for audio data
        self._receive_queue: asyncio.Queue[
            tuple[
                bool,
                int,
                str | bytes | TencentTTSTaskFailedException | int | None,
            ]
        ] = asyncio.Queue()

    async def start(self) -> None:
        """Start the TTS client and initialize components."""

        # Create synthesizer with configuration
        self._callback = AsyncIteratorCallback(
            self.ten_env, self._receive_queue
        )

        credential_var = credential.Credential(
            self.config.secret_id, self.config.secret_key
        )

        synthesizer = FlowingSpeechSynthesizer(
            self.config.app_id, credential_var, self._callback
        )

        synthesizer.set_voice_type(self.config.voice_type)
        synthesizer.set_codec(self.config.codec)
        synthesizer.set_sample_rate(self.config.sample_rate)
        synthesizer.set_enable_subtitle(self.config.enable_subtitle)
        synthesizer.set_speed(self.config.speed)
        synthesizer.set_volume(self.config.volume)
        synthesizer.set_emotion_category(self.config.emotion_category)
        synthesizer.set_emotion_intensity(self.config.emotion_intensity)

        try:
            synthesizer.start()
            synthesizer.wait_ready(5000)
        except Exception as e:
            self.ten_env.log_error(f"Error starting TTS: {e}")
            raise e

        self.synthesizer = synthesizer
        self.conn_ready_event.set()

        self.ten_env.log_debug("Tencent TTS client started successfully")

    async def stop(self) -> None:
        """Stop the TTS client and clean up resources."""
        self.close()
        # restart the synthesizer
        asyncio.create_task(self.start())

    def close(self) -> None:
        """
        Close the TTS client and clean up resources.
        """
        if self.synthesizer:
            try:
                if self.synthesizer.is_alive():
                    self.synthesizer.close()
                else:
                    self.ten_env.log_debug(
                        "Synthesizer is not alive, skipping close"
                    )
                self.ten_env.log_debug("TTS operation closed")
            except Exception as e:
                self.ten_env.log_error(f"Error cancelling TTS: {e}")

            # Clean up synthesizer
            self.synthesizer = None
            self.conn_ready_event.clear()

    def complete(self) -> None:
        """
        Complete current TTS operation.
        """
        self.ten_env.log_debug("TTS operation completed")
        if self.synthesizer and self.synthesizer.is_alive():
            try:
                self.synthesizer.complete()
                self.conn_ready_event.clear()

                self.ten_env.log_debug("TTS operation completed")
            except Exception as e:
                self.ten_env.log_error(f"Error completing TTS: {e}")

    async def synthesize_audio(self, text: str, text_input_end: bool):
        """
        Start audio synthesis for the given text.
        This method only initiates synthesis and returns immediately.
        Audio data should be consumed from the queue independently.
        """
        self.ten_env.log_debug(
            f"Starting TTS synthesis, text: {text}, input_end: {text_input_end},conn_ready_event: {self.conn_ready_event.is_set()}"
        )

        await self.conn_ready_event.wait()
        if not self.synthesizer or not self.synthesizer.is_alive():
            self.ten_env.log_error("Synthesizer is not alive, reinitializing")
            await self.start()

        # Start streaming TTS synthesis
        self._callback.set_sent_ts()
        self.synthesizer.process(text)

        self.ten_env.log_debug(f"TTS synthesis initiated for text: {text}")

    async def get_audio_data(self):
        """
        Get audio data from the queue. This is a separate method that can be called
        independently to consume audio data.
        Returns: (done, message_type, data)
        """
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
