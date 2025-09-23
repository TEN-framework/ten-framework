import asyncio
from datetime import datetime

import dashscope
from dashscope.audio.tts import (
    SpeechSynthesizer,
    ResultCallback,
    SpeechSynthesisResult,
)

from .config import SambertTTSConfig
from ten_runtime.async_ten_env import AsyncTenEnv


MESSAGE_TYPE_PCM = 1
MESSAGE_TYPE_CMD_COMPLETE = 2
MESSAGE_TYPE_CMD_ERROR = 3
MESSAGE_TYPE_CMD_CANCEL = 4

ERROR_CODE_TTS_FAILED = -1

# Audio format mapping constants for new SDK
AUDIO_FORMAT_MAPPING = {
    8000: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 8000,
    },
    16000: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 16000,
    },
    22050: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 22050,
    },
    24000: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 24000,
    },
    44100: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 44100,
    },
    48000: {
        "format": SpeechSynthesizer.AudioFormat.format_pcm,
        "sample_rate": 48000,
    },
}
DEFAULT_AUDIO_FORMAT = {
    "format": SpeechSynthesizer.AudioFormat.format_pcm,
    "sample_rate": 16000,
}


class SambertTTSTaskFailedException(Exception):
    """Exception raised when Sambert TTS task fails"""

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
        self._cancelled = False
        self._loop = asyncio.get_event_loop()
        self._queue = queue

    def close(self):
        """Close the callback."""
        self._closed = True

    def cancel(self):
        """Cancel the callback, filtering out further audio output."""
        self._cancelled = True
        self.ten_env.log_info(
            "AsyncIteratorCallback cancelled, will filter audio output"
        )

    def on_open(self):
        """Called when WebSocket connection opens."""
        self.ten_env.log_info("WebSocket connection opened for TTS synthesis.")

    def on_complete(self):
        """Called when TTS synthesis completes successfully."""
        self.ten_env.log_info("TTS synthesis task completed successfully.")

        # Send completion signal only if not cancelled
        if not self._cancelled:
            asyncio.run_coroutine_threadsafe(
                self._queue.put((True, MESSAGE_TYPE_CMD_COMPLETE, None)),
                self._loop,
            )

    def on_error(self, response) -> None:
        """Called when TTS synthesis encounters an error."""
        error_msg = f"TTS synthesis task failed: {response}"
        self.ten_env.log_error(error_msg)

        # Send error signal only if not cancelled
        if not self._cancelled:
            asyncio.run_coroutine_threadsafe(
                self._queue.put((True, MESSAGE_TYPE_CMD_ERROR, error_msg)),
                self._loop,
            )

    def on_close(self):
        """Called when WebSocket connection closes."""
        self.ten_env.log_info("WebSocket connection closed.")
        self.close()

    def on_event(self, result: SpeechSynthesisResult) -> None:
        """Called when receiving events from TTS service."""
        if self._closed or self._cancelled:
            if self._cancelled:
                # 计算过滤的音频数据大小
                audio_frame = result.get_audio_frame()
                if audio_frame:
                    self.ten_env.log_debug(
                        f"Filtered {len(audio_frame)} bytes audio data due to cancellation"
                    )
            return

        # 处理音频帧数据
        audio_frame = result.get_audio_frame()
        if audio_frame:
            self.ten_env.log_info(
                f"Received audio data: {len(audio_frame)} bytes"
            )
            # Send audio data to queue
            asyncio.run_coroutine_threadsafe(
                self._queue.put((False, MESSAGE_TYPE_PCM, audio_frame)),
                self._loop,
            )

        # 处理时间戳数据
        timestamp = result.get_timestamp()
        if timestamp:
            self.ten_env.log_debug(f"Received timestamp: {timestamp}")

        # 处理响应信息
        response = result.get_response()
        if response:
            self.ten_env.log_debug(f"Received response: {response}")


class SambertTTSClient:
    """Client for Sambert TTS service using dashscope."""

    def __init__(
        self,
        config: SambertTTSConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
    ):
        # Configuration and environment
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor

        # TTS callback
        self._callback: AsyncIteratorCallback | None = None
        self._synthesis_task: asyncio.Task | None = None

        # Communication queue for audio data
        self._receive_queue: asyncio.Queue[
            tuple[bool, int, str | bytes | None]
        ] = asyncio.Queue()

        # Set dashscope API key
        dashscope.api_key = config.api_key

    def start(self) -> None:
        """Start the TTS client and initialize components."""

        # Initialize audio data queue
        self._callback = AsyncIteratorCallback(
            self.ten_env, self._receive_queue
        )

        self.ten_env.log_info("Sambert TTS client started successfully")

    def cancel(self) -> None:
        """
        Cancel current TTS operation.
        """
        # 首先设置callback的取消标识位，过滤后续音频输出
        if self._callback:
            self._callback.cancel()

        # 取消正在进行的合成任务
        if self._synthesis_task and not self._synthesis_task.done():
            self._synthesis_task.cancel()
            self.ten_env.log_info("TTS synthesis task cancelled")

    def complete(self) -> None:
        """
        Complete current TTS operation.
        """
        # 对于同步API，complete操作主要是等待任务完成
        if self._synthesis_task and not self._synthesis_task.done():
            self.ten_env.log_info("Waiting for TTS synthesis to complete")
        else:
            self.ten_env.log_info("TTS synthesis already completed")

    def synthesize_audio(self, text: str, text_input_end: bool):
        """
        Start audio synthesis for the given text.
        This method only initiates synthesis and returns immediately.
        Audio data should be consumed from the queue independently.
        """
        self.ten_env.log_info(
            f"Starting TTS synthesis, text: {text}, input_end: {text_input_end}"
        )

        # Start callback if not initialized
        if self._callback is None:
            self.ten_env.log_info(
                "Callback is not initialized, starting new one."
            )
            self.start()

        # 异步执行TTS合成
        self._synthesis_task = asyncio.create_task(self._async_synthesize(text))

    async def _async_synthesize(self, text: str):
        """异步执行TTS合成"""
        try:
            audio_format = self._get_audio_format()

            # 在线程池中执行同步的TTS调用
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(
                None,
                lambda: SpeechSynthesizer.call(
                    model=self.config.model,
                    text=text,
                    callback=self._callback,
                    **audio_format,
                ),
            )

            self.ten_env.log_info("TTS synthesis completed successfully")

        except Exception as e:
            self.ten_env.log_error(f"TTS synthesis failed: {e}")
            # 发送错误信号到队列
            if self._callback:
                await self._receive_queue.put(
                    (True, MESSAGE_TYPE_CMD_ERROR, str(e))
                )

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

    def _get_audio_format(self) -> dict:
        """
        Automatically generate AudioFormat based on configuration.

        Returns:
            dict: The appropriate audio format parameters for the configuration
        """
        if self.config.sample_rate in AUDIO_FORMAT_MAPPING:
            return AUDIO_FORMAT_MAPPING[self.config.sample_rate]

        # Fallback to default format if configuration not supported
        self.ten_env.log_warn(
            f"Unsupported audio format: {self.config.sample_rate}Hz, using default format: PCM_16000HZ_MONO_16BIT"
        )
        return DEFAULT_AUDIO_FORMAT
