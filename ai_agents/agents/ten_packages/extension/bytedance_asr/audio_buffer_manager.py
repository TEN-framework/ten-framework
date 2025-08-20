from typing import Callable, Any, Awaitable, Union, Optional
import asyncio
from collections import deque


class AudioBufferManager:
    """
    Manages audio data buffering for Bytedance ASR using queue-based approach.

    Features:
    - Queue-based buffer with configurable max size
    - Automatic memory management (FIFO when limit exceeded)
    - Support for both sync and async callbacks
    - Configurable threshold for sending
    - Better handling of reconnection scenarios
    """

    def __init__(
        self,
        threshold_bytes: int = 4800,  # 4800 bytes for 150ms at 16kHz
        max_buffer_size: int = 1024 * 1024,  # 1MB max buffer size
        logger=None,
    ):
        self.threshold_bytes = threshold_bytes
        self.max_buffer_size = max_buffer_size
        self.logger = logger

        # Queue-based buffer
        self.audio_queue: deque = deque()
        self.current_buffer_size: int = 0
        self.total_bytes_sent: int = 0
        self.total_bytes_dropped: int = 0

    def reset(self):
        """Reset buffer"""
        self.audio_queue.clear()
        self.current_buffer_size = 0
        if self.logger:
            self.logger.log_debug("Audio buffer reset")

    def get_buffer_size(self) -> int:
        """Get current buffer size in bytes"""
        return self.current_buffer_size

    def get_queue_length(self) -> int:
        """Get number of audio chunks in queue"""
        return len(self.audio_queue)

    def _add_to_queue(self, audio_data: bytes) -> None:
        """Add audio data to queue with memory management"""
        # Check if adding this data would exceed max buffer size
        while (
            self.current_buffer_size + len(audio_data) > self.max_buffer_size
            and self.audio_queue
        ):
            # Remove oldest data (FIFO)
            removed_data = self.audio_queue.popleft()
            self.current_buffer_size -= len(removed_data)
            self.total_bytes_dropped += len(removed_data)

            if self.logger:
                self.logger.log_debug(
                    f"Dropped {len(removed_data)} bytes due to buffer limit"
                )

        # Add new data
        self.audio_queue.append(audio_data)
        self.current_buffer_size += len(audio_data)

    def _get_audio_chunk(self) -> Optional[bytes]:
        """Get audio chunk from queue that meets threshold"""
        if not self.audio_queue:
            return None

        # If single chunk meets threshold, return it
        if len(self.audio_queue[0]) >= self.threshold_bytes:
            chunk = self.audio_queue.popleft()
            self.current_buffer_size -= len(chunk)
            return chunk

        # Combine chunks to meet threshold
        combined_chunk = bytearray()
        while self.audio_queue and len(combined_chunk) < self.threshold_bytes:
            chunk = self.audio_queue.popleft()
            combined_chunk.extend(chunk)
            self.current_buffer_size -= len(chunk)

        return bytes(combined_chunk) if combined_chunk else None

    async def push_audio(
        self,
        audio_data: bytes,
        send_callback: Union[
            Callable[[bytes], Any], Callable[[bytes], Awaitable[Any]]
        ],
        force_send: bool = False,
    ) -> bool:
        """
        Push audio data to queue and send if threshold is reached or force_send is True.

        Args:
            audio_data: Audio data bytes
            send_callback: Callback function to send audio data (sync or async)
            force_send: Force send buffer even if threshold is not reached

        Returns:
            True if data was sent, False otherwise
        """
        # Add data to queue
        self._add_to_queue(audio_data)

        # Check if we should send data
        should_send = (
            force_send or self.current_buffer_size >= self.threshold_bytes
        )

        if should_send:
            # Get audio chunk to send
            audio_chunk = self._get_audio_chunk()
            if audio_chunk:
                # Check if callback is async
                if asyncio.iscoroutinefunction(send_callback):
                    await send_callback(audio_chunk)
                else:
                    send_callback(audio_chunk)

                # Update stats
                self.total_bytes_sent += len(audio_chunk)
                return True

        return False

    async def flush(
        self,
        send_callback: Union[
            Callable[[bytes], Any], Callable[[bytes], Awaitable[Any]]
        ],
    ) -> bool:
        """
        Flush all remaining audio data in queue.

        Args:
            send_callback: Callback function to send audio data

        Returns:
            True if data was sent, False if queue was empty
        """
        if not self.audio_queue:
            return False

        # Combine all remaining chunks
        combined_chunk = bytearray()
        while self.audio_queue:
            chunk = self.audio_queue.popleft()
            combined_chunk.extend(chunk)
            self.current_buffer_size -= len(chunk)

        if combined_chunk:
            # Check if callback is async
            if asyncio.iscoroutinefunction(send_callback):
                await send_callback(bytes(combined_chunk))
            else:
                send_callback(bytes(combined_chunk))

            # Update stats
            self.total_bytes_sent += len(combined_chunk)
            return True

        return False

    def get_stats(self) -> dict:
        """Get buffer statistics"""
        return {
            "current_buffer_size": self.current_buffer_size,
            "queue_length": len(self.audio_queue),
            "total_bytes_sent": self.total_bytes_sent,
            "total_bytes_dropped": self.total_bytes_dropped,
            "max_buffer_size": self.max_buffer_size,
            "threshold_bytes": self.threshold_bytes,
        }

    def clear_old_data(self, max_age_ms: int = 5000) -> int:
        """
        Clear old audio data based on estimated age.
        This is useful for reconnection scenarios to avoid sending very old data.

        Args:
            max_age_ms: Maximum age in milliseconds to keep

        Returns:
            Number of bytes cleared
        """
        # Estimate bytes per millisecond (16kHz * 2 bytes per sample / 1000ms)
        bytes_per_ms = 32  # 16000 * 2 / 1000
        max_age_bytes = max_age_ms * bytes_per_ms

        cleared_bytes = 0
        while self.audio_queue and self.current_buffer_size > max_age_bytes:
            removed_data = self.audio_queue.popleft()
            self.current_buffer_size -= len(removed_data)
            cleared_bytes += len(removed_data)

        if cleared_bytes > 0 and self.logger:
            self.logger.log_debug(
                f"Cleared {cleared_bytes} bytes of old audio data"
            )

        return cleared_bytes
