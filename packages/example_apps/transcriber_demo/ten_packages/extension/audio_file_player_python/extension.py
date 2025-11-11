#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os
from pathlib import Path
from typing_extensions import override
from ten_runtime import (
    AudioFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    LogLevel,
    AudioFrameDataFmt,
)
from pydub import AudioSegment


class AudioFilePlayerExtension(AsyncExtension):
    """
    Audio File Player Extension

    Features:
    1. Receive start_play command with file_path property
    2. Load and convert audio to 16000Hz sample rate PCM
    3. Send audio frames at 10ms intervals
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.audio_file_path: str | None = None
        self.is_playing: bool = False
        self.play_task: asyncio.Task[None] | None = None  # type: ignore
        self.loop_playback: bool = False

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info(
            "AudioFilePlayerExtension on_init", category="key_point"
        )

    @override
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info(
            "AudioFilePlayerExtension on_start", category="key_point"
        )
        # Wait for start_play command to begin playback

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info(
            "AudioFilePlayerExtension on_stop", category="key_point"
        )

        # Stop playback
        self.is_playing = False
        if self.play_task:
            self.play_task.cancel()
            try:
                await self.play_task
            except asyncio.CancelledError:
                pass

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info(
            "AudioFilePlayerExtension on_deinit", category="key_point"
        )

    @override
    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        """Handle commands"""
        cmd_name = cmd.get_name()
        ten_env.log(LogLevel.DEBUG, f"Received command: {cmd_name}")

        if cmd_name == "start_play":
            # Get file_path from command property
            file_path_result, error = cmd.get_property_string("file_path")

            if error is not None:
                ten_env.log(
                    LogLevel.ERROR,
                    f"Failed to get file_path from command: {error}",
                )
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
                cmd_result.set_property_string(
                    "detail", "Missing file_path property in command"
                )
                await ten_env.return_result(cmd_result)
                return

            self.audio_file_path = file_path_result

            # Get optional loop_playback parameter
            loop_playback_result, error = cmd.get_property_bool("loop_playback")
            if error is None:
                self.loop_playback = loop_playback_result
            else:
                # Default to false if not specified
                self.loop_playback = False

            ten_env.log_info(
                f"Start playing: file_path={self.audio_file_path}, loop={self.loop_playback}",
                category="key_point",
            )

            # Verify file exists
            if not os.path.exists(self.audio_file_path):
                ten_env.log_error(
                    f"Audio file does not exist: {self.audio_file_path}",
                    category="key_point",
                )
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
                cmd_result.set_property_string(
                    "detail", f"File not found: {self.audio_file_path}"
                )
                await ten_env.return_result(cmd_result)
                return

            # Start playback
            if not self.is_playing:
                self.is_playing = True
                self.play_task = asyncio.create_task(self._play_audio(ten_env))
                cmd_result = CmdResult.create(StatusCode.OK, cmd)
                cmd_result.set_property_string("detail", "Playback started")
            else:
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
                cmd_result.set_property_string("detail", "Already playing")

        elif cmd_name == "stop_play":
            # Stop playback
            if self.is_playing:
                self.is_playing = False
                if self.play_task:
                    self.play_task.cancel()
                ten_env.log_info("Playback stopped", category="key_point")
                cmd_result = CmdResult.create(StatusCode.OK, cmd)
                cmd_result.set_property_string("detail", "Playback stopped")
            else:
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
                cmd_result.set_property_string(
                    "detail", "Not currently playing"
                )

        else:
            cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
            cmd_result.set_property_string(
                "detail", f"Unknown command: {cmd_name}"
            )

        await ten_env.return_result(cmd_result)

    async def _play_audio(self, ten_env: AsyncTenEnv) -> None:
        """Main audio playback loop"""
        try:
            while self.is_playing:
                # Load audio file
                ten_env.log_info(
                    f"Loading audio file: {self.audio_file_path}",
                    category="key_point",
                )
                audio = await self._load_audio_file(ten_env)

                if audio is None:
                    ten_env.log_error(
                        "Audio loading failed", category="key_point"
                    )
                    break

                # Convert to 16000Hz mono
                audio = audio.set_frame_rate(16000).set_channels(1)  # type: ignore
                ten_env.log_info(
                    f"Audio converted: sample_rate=16000Hz, channels=1, duration={len(audio)}ms",
                )

                # Calculate parameters
                sample_rate = 16000
                bytes_per_sample = 2  # 16-bit PCM
                channels = 1
                frame_duration_ms = 10
                samples_per_frame = int(
                    sample_rate * frame_duration_ms / 1000
                )  # 160 samples
                bytes_per_frame = (
                    samples_per_frame * bytes_per_sample * channels
                )  # 320 bytes

                # Get audio raw data
                raw_data = audio.raw_data  # type: ignore
                if raw_data is None:
                    ten_env.log_error(
                        "Failed to get audio raw data", category="key_point"
                    )
                    break
                total_frames = len(raw_data) // bytes_per_frame

                ten_env.log_info(
                    f"Start sending audio frames: total_frames={total_frames}, bytes_per_frame={bytes_per_frame}",
                    category="key_point",
                )

                # Send frames one by one
                for frame_idx in range(total_frames):
                    if not self.is_playing:
                        ten_env.log_info(
                            "Playback interrupted", category="key_point"
                        )
                        break

                    # Extract current frame data
                    start_byte = frame_idx * bytes_per_frame
                    end_byte = start_byte + bytes_per_frame
                    frame_data = raw_data[start_byte:end_byte]

                    # Create audio frame
                    audio_frame = AudioFrame.create("pcm_frame")
                    audio_frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
                    audio_frame.set_bytes_per_sample(bytes_per_sample)
                    audio_frame.set_sample_rate(sample_rate)
                    audio_frame.set_number_of_channels(channels)
                    audio_frame.set_samples_per_channel(samples_per_frame)
                    audio_frame.set_timestamp(
                        frame_idx * frame_duration_ms * 1000
                    )  # microseconds
                    audio_frame.set_eof(False)

                    # Fill data
                    audio_frame.alloc_buf(len(frame_data))
                    buf = audio_frame.lock_buf()
                    buf[:] = frame_data
                    audio_frame.unlock_buf(buf)

                    # Send audio frame
                    await ten_env.send_audio_frame(audio_frame)

                    # Wait 10ms (simulate real-time playback)
                    await asyncio.sleep(frame_duration_ms / 1000.0)

                # Playback completed
                if self.is_playing:
                    ten_env.log_info(
                        f"Audio playback completed, sent {total_frames} frames",
                        category="key_point",
                    )

                # Exit if not looping
                if not self.loop_playback:
                    self.is_playing = False
                    break

                # Continue to next round if looping
                if self.loop_playback and self.is_playing:
                    ten_env.log_info(
                        "Loop playback: restarting", category="key_point"
                    )

        except asyncio.CancelledError:
            ten_env.log_info("Playback task cancelled", category="key_point")
        except Exception as e:
            ten_env.log_error(
                f"Error during audio playback: {e}", category="key_point"
            )
            import traceback

            ten_env.log_error(traceback.format_exc())

    async def _load_audio_file(
        self, ten_env: AsyncTenEnv
    ) -> AudioSegment | None:  # type: ignore
        """Load audio file, supports wav/mp3/pcm formats"""
        try:
            if not self.audio_file_path:
                ten_env.log_error(
                    "Audio file path is not set", category="key_point"
                )
                return None

            file_path = Path(self.audio_file_path)
            file_ext = file_path.suffix.lower()

            # Load based on file extension
            if file_ext in [".wav", ".mp3", ".ogg", ".flac", ".m4a"]:
                # pydub automatically recognizes format
                audio = AudioSegment.from_file(self.audio_file_path)  # type: ignore
            elif file_ext == ".pcm":
                # PCM format requires parameters (assume 16kHz, 16-bit, mono)
                audio = AudioSegment.from_file(  # type: ignore
                    self.audio_file_path,
                    format="s16le",
                    sample_width=2,
                    channels=1,
                    frame_rate=16000,
                )
            else:
                ten_env.log_error(
                    f"Unsupported audio format: {file_ext}",
                    category="key_point",
                )
                return None

            return audio  # type: ignore

        except Exception as e:
            ten_env.log_error(
                f"Failed to load audio file: {e}", category="key_point"
            )
            import traceback

            ten_env.log_error(traceback.format_exc())
            return None
