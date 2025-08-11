from typing import Callable, Any, Awaitable, Union
import asyncio


class AudioBufferManager:
    """
    Manages audio data buffering with fixed threshold.

    Features:
    - Fixed threshold (default: 1280 bytes)
    - Automatic buffer management
    - Buffer flushing on demand
    - Support for both sync and async callbacks
    - Detailed logging for monitoring and debugging
    """

    def __init__(
        self,
        threshold_bytes: int = 1280,  # 1280 bytes
        logger=None,
    ):
        self.threshold_bytes = threshold_bytes
        self.logger = logger

        # State tracking
        self.buffer: bytearray = bytearray()
        self.total_bytes_sent: int = 0

    def reset(self):
        """Reset buffer"""
        self.buffer = bytearray()
        if self.logger:
            self.logger.log_debug("Audio buffer reset")

    def get_buffer_size(self) -> int:
        """Get current buffer size in bytes"""
        return len(self.buffer)

    async def push_audio(
        self,
        audio_data: bytes,
        send_callback: Union[Callable[[bytes], Any], Callable[[bytes], Awaitable[Any]]],
        force_send: bool = False,
    ) -> bool:
        """
        Push audio data to buffer and send if threshold is reached or force_send is True.

        Args:
            audio_data: Audio data bytes
            send_callback: Callback function to send audio data (sync or async)
            force_send: Force send buffer even if threshold is not reached

        Returns:
            True if data was sent, False otherwise
        """
        # Add data to buffer
        self.buffer.extend(audio_data)

        # Check if we should send data
        should_send = force_send or len(self.buffer) >= self.threshold_bytes

        if should_send:
            # if self.logger:
            #     self.logger.log_debug(
            #         f"Sending audio data: {len(self.buffer)} bytes "
            #         f"(force_send: {force_send}, threshold: {self.threshold_bytes})"
            #     )

            # Send buffer
            buffer_copy = bytes(self.buffer)

            # Check if callback is async
            if asyncio.iscoroutinefunction(send_callback):
                await send_callback(buffer_copy)
            else:
                send_callback(buffer_copy)

            # Update stats
            self.total_bytes_sent += len(self.buffer)

            # Clear buffer
            self.buffer = bytearray()

            return True

        # if self.logger:
        #     self.logger.log_debug(
        #         f"Buffering audio data: {len(self.buffer)}/{self.threshold_bytes} bytes"
        #     )

        return False

    async def flush(
        self,
        send_callback: Union[Callable[[bytes], Any], Callable[[bytes], Awaitable[Any]]],
    ) -> bool:
        """
        Flush buffer and send all data.

        Args:
            send_callback: Callback function to send audio data (sync or async)

        Returns:
            True if data was sent, False if buffer was empty
        """
        if not self.buffer:
            if self.logger:
                self.logger.log_debug("Audio buffer is empty, nothing to flush")
            return False

        if self.logger:
            self.logger.log_debug(f"Flushing audio buffer: {len(self.buffer)} bytes")

        # Send buffer
        buffer_copy = bytes(self.buffer)

        # Check if callback is async
        if asyncio.iscoroutinefunction(send_callback):
            await send_callback(buffer_copy)
        else:
            send_callback(buffer_copy)

        # Update stats
        self.total_bytes_sent += len(self.buffer)

        # Clear buffer
        self.buffer = bytearray()

        return True
