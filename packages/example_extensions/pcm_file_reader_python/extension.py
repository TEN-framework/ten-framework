#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from ten_runtime import (
    AudioFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    LogLevel,
)


class PcmFileReaderExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self._audio_file_path: str = ""
        self._sample_rate: int = 16000
        self._bytes_per_sample: int = 2
        self._number_of_channels: int = 1
        self._frame_interval_ms: int = 10
        self._audio_frame_name: str = "pcm_frame"
        self._pcm_data: bytes = b""
        self._is_running: bool = False
        self._send_task: asyncio.Task[None] | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")

        # Read properties
        try:
            audio_file_path, _ = await ten_env.get_property_string(
                "audio_file_path"
            )
            if audio_file_path:
                self._audio_file_path = audio_file_path
        except Exception as e:
            ten_env.log(
                LogLevel.WARN,
                f"Failed to get audio_file_path property: {e}",
            )

        try:
            sample_rate, _ = await ten_env.get_property_int("sample_rate")
            if sample_rate > 0:
                self._sample_rate = sample_rate
        except Exception:
            pass

        try:
            bytes_per_sample, _ = await ten_env.get_property_int(
                "bytes_per_sample"
            )
            if bytes_per_sample > 0:
                self._bytes_per_sample = bytes_per_sample
        except Exception:
            pass

        try:
            number_of_channels, _ = await ten_env.get_property_int(
                "number_of_channels"
            )
            if number_of_channels > 0:
                self._number_of_channels = number_of_channels
        except Exception:
            pass

        try:
            frame_interval_ms, _ = await ten_env.get_property_int(
                "frame_interval_ms"
            )
            if frame_interval_ms > 0:
                self._frame_interval_ms = frame_interval_ms
        except Exception:
            pass

        ten_env.log(
            LogLevel.INFO,
            f"PCM File Reader config: "
            f"audio_file_path={self._audio_file_path}, "
            f"sample_rate={self._sample_rate}, "
            f"bytes_per_sample={self._bytes_per_sample}, "
            f"number_of_channels={self._number_of_channels}, "
            f"frame_interval_ms={self._frame_interval_ms}",
        )

        # Read PCM file
        if self._audio_file_path:
            try:
                with open(self._audio_file_path, "rb") as f:
                    self._pcm_data = f.read()
                ten_env.log(
                    LogLevel.INFO,
                    f"Loaded PCM file: {self._audio_file_path}, "
                    f"size={len(self._pcm_data)} bytes",
                )
            except Exception as e:
                ten_env.log(
                    LogLevel.ERROR,
                    f"Failed to read PCM file {self._audio_file_path}: {e}",
                )
                return

            # Start sending audio frames
            self._is_running = True
            self._send_task = asyncio.create_task(
                self._send_audio_frames(ten_env)
            )
        else:
            ten_env.log(
                LogLevel.WARN,
                "No audio_file_path specified, extension will not send frames",
            )

    async def _send_audio_frames(self, ten_env: AsyncTenEnv) -> None:
        """Send audio frames at specified interval."""
        # Calculate frame size: samples_per_frame * bytes_per_sample * channels
        # samples_per_frame = sample_rate * frame_interval_ms / 1000
        samples_per_frame = self._sample_rate * self._frame_interval_ms // 1000
        frame_size = (
            samples_per_frame
            * self._bytes_per_sample
            * self._number_of_channels
        )

        ten_env.log(
            LogLevel.INFO,
            f"Frame size: {frame_size} bytes, "
            f"samples_per_frame: {samples_per_frame}",
        )

        offset = 0
        timestamp = 0

        while self._is_running and offset < len(self._pcm_data):
            # Get frame data
            remaining = len(self._pcm_data) - offset
            current_frame_size = min(frame_size, remaining)
            frame_data = self._pcm_data[offset : offset + current_frame_size]

            # Create audio frame
            audio_frame = AudioFrame.create(self._audio_frame_name)
            audio_frame.alloc_buf(len(frame_data))

            buf = audio_frame.lock_buf()
            buf[:] = frame_data
            audio_frame.unlock_buf(buf)

            # Set audio frame properties
            audio_frame.set_sample_rate(self._sample_rate)
            audio_frame.set_bytes_per_sample(self._bytes_per_sample)
            audio_frame.set_number_of_channels(self._number_of_channels)
            audio_frame.set_samples_per_channel(
                current_frame_size
                // self._bytes_per_sample
                // self._number_of_channels
            )
            audio_frame.set_timestamp(timestamp)
            audio_frame.set_data_fmt(0)  # Interleaved format
            audio_frame.set_line_size(current_frame_size)

            # Check if this is the last frame
            is_eof = offset + current_frame_size >= len(self._pcm_data)
            audio_frame.set_eof(is_eof)

            # Send the audio frame
            await ten_env.send_audio_frame(audio_frame)

            ten_env.log(
                LogLevel.DEBUG,
                f"Sent audio frame: offset={offset}, "
                f"size={current_frame_size}, timestamp={timestamp}, eof={is_eof}",
            )

            offset += current_frame_size
            timestamp += (
                self._frame_interval_ms * 1000
            )  # Convert to microseconds

            # Wait for next frame interval
            if self._is_running and offset < len(self._pcm_data):
                await asyncio.sleep(self._frame_interval_ms / 1000.0)

        ten_env.log(
            LogLevel.INFO,
            "Finished sending all audio frames",
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_stop")

        # Stop the sending task
        self._is_running = False
        if self._send_task:
            self._send_task.cancel()
            try:
                await self._send_task
            except asyncio.CancelledError:
                pass
            self._send_task = None

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log(LogLevel.DEBUG, f"on_cmd name {cmd_name}")

        if cmd_name == "get_status":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_string(
                "status", "running" if self._is_running else "stopped"
            )
            await ten_env.return_result(cmd_result)
        else:
            cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
            cmd_result.set_property_string(
                "detail", f"Unknown command: {cmd_name}"
            )
            await ten_env.return_result(cmd_result)
