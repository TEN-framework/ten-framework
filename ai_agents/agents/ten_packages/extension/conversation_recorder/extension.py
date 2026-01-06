from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    StatusCode,
    CmdResult,
)
import asyncio
import json
import logging
import signal
import atexit
from .audio_mixer import AudioMixer
from .storage import StorageFactory


# Global registry of active recorders for signal handling
_active_recorders = []


def _emergency_close_all():
    """Emergency close all active recorders - called on signals/atexit."""
    for recorder in _active_recorders:
        recorder._emergency_close()


def _signal_handler(signum, frame):
    """Handle SIGTERM/SIGINT to save recordings before exit."""
    _emergency_close_all()
    # Re-raise to allow normal shutdown
    raise SystemExit(128 + signum)


class ConversationRecorderExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config = {}
        self.mixer = None
        self.storage = None
        self.is_recording = False
        self.recording_task = None
        self.loop = None
        self.users_count = 0
        self._flush_counter = 0
        self._signals_registered = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("ConversationRecorderExtension on_init")
        config_json, _ = await ten_env.get_property_to_json()
        if config_json:
            self.config = json.loads(config_json)

        # Get sample rate from config, default to 24000Hz (Gemini output rate)
        sample_rate = self.config.get("sample_rate", 24000)

        self.mixer = AudioMixer(sample_rate=sample_rate)
        self.storage = StorageFactory.create_storage(
            self.config.get("storage_type", "local"), self.config
        )

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("ConversationRecorderExtension on_start")
        self.loop = asyncio.get_running_loop()

        # Register for emergency shutdown handling
        _active_recorders.append(self)
        self._register_signal_handlers(ten_env)

        trigger = self.config.get("start_trigger")
        if trigger == "on_start":
            await self.start_recording(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("ConversationRecorderExtension on_stop")
        await self.stop_recording(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("ConversationRecorderExtension on_deinit")
        # Remove from active recorders
        if self in _active_recorders:
            _active_recorders.remove(self)

    def _register_signal_handlers(self, ten_env: AsyncTenEnv):
        """Register signal handlers for graceful shutdown."""
        if self._signals_registered:
            return
        try:
            signal.signal(signal.SIGTERM, _signal_handler)
            signal.signal(signal.SIGINT, _signal_handler)
            atexit.register(_emergency_close_all)
            self._signals_registered = True
            ten_env.log_info("Signal handlers registered for graceful shutdown")
        except Exception as e:
            ten_env.log_warn(f"Could not register signal handlers: {e}")

    def _emergency_close(self):
        """Synchronously close storage - called from signal handlers."""
        if self.storage:
            try:
                self.storage.close()
            except Exception:
                pass  # Best effort

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"Received cmd: {cmd_name}")

        if cmd_name == "on_user_joined":
            self.users_count += 1
            trigger = self.config.get("start_trigger", "on_user_joined")
            if trigger == "on_user_joined":
                await self.start_recording(ten_env)

        elif cmd_name == "on_user_left":
            self.users_count = max(0, self.users_count - 1)
            if self.users_count == 0:
                await self.stop_recording(ten_env)

        elif cmd_name == "flush":
            # For this MVP, we rely on the loop to drain the mixer eventually.
            pass

        # Return success for commands we handle - don't forward
        result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(result)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, frame: AudioFrame
    ) -> None:
        if not self.is_recording:
            await ten_env.send_audio_frame(frame)
            return

        stream_id, valid = frame.get_property_int("stream_id")
        if not valid:
            stream_id = 0

        # Get the sample rate from the audio frame for proper resampling
        source_sample_rate = frame.get_sample_rate()

        buf = frame.lock_buf()
        try:
            # We must copy the buffer because we unlock it immediately
            data = bytes(buf)
            self.mixer.push_audio(str(stream_id), data, source_sample_rate)
        finally:
            frame.unlock_buf(buf)

        await ten_env.send_audio_frame(frame)

    async def start_recording(self, ten_env: AsyncTenEnv):
        if self.is_recording:
            return

        ten_env.log_info("Starting recording session...")
        self.is_recording = True

        # Open storage in executor to avoid blocking
        if self.storage:
            await self.loop.run_in_executor(None, self.storage.open)
            if hasattr(self.storage, "actual_file_path"):
                ten_env.log_info(
                    f"Recording to file: {self.storage.actual_file_path}"
                )

        self.recording_task = asyncio.create_task(self._recording_loop(ten_env))

    async def stop_recording(self, ten_env: AsyncTenEnv):
        if not self.is_recording:
            return

        ten_env.log_info("Stopping recording session...")
        self.is_recording = False
        if self.recording_task:
            await self.recording_task
            self.recording_task = None

        if self.storage:
            file_path = getattr(self.storage, "actual_file_path", None)
            await self.loop.run_in_executor(None, self.storage.close)
            ten_env.log_info(f"Recording saved to: {file_path}")

    async def _recording_loop(self, ten_env: AsyncTenEnv):
        while self.is_recording:
            try:
                # Sleep approx one chunk duration (40ms)
                await asyncio.sleep(0.04)

                # Mix audio
                mixed_bytes = self.mixer.mix_next_chunk()

                if mixed_bytes and self.storage:
                    # Write in thread pool
                    await self.loop.run_in_executor(
                        None, self.storage.write, mixed_bytes
                    )

                # Flush periodically (~every 1 second = 25 chunks at 40ms each)
                self._flush_counter += 1
                if self._flush_counter >= 25:
                    self._flush_counter = 0
                    if self.storage:
                        await self.loop.run_in_executor(
                            None, self.storage.flush
                        )

            except Exception as e:
                ten_env.log_error(f"Error in recording loop: {e}")
