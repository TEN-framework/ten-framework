#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import base64
import traceback

from ten_runtime import (
    AudioFrame,
    VideoFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from .generic import AgoraGenericRecorder
from .config import GenericVideoConfig

# Error codes
ERROR_CODE_CONFIG_VALIDATION_ERROR = -1012


class GenericVideoExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: GenericVideoConfig | None = None
        self.input_audio_queue = asyncio.Queue()
        self.recorder: AgoraGenericRecorder = None
        self.ten_env: AsyncTenEnv = None

        self._audio_processing_enabled = False  # Control audio processing loop
        self._audio_task = None
        self._config_valid = False  # Track configuration validation status
        self._connection_task = None
        self._sample_rate_mismatch_warned = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        try:
            self.config = await GenericVideoConfig.create_async(ten_env)
            self._config_valid = True

            ten_env.log_info(
                "[GENERIC-VIDEO] Config: "
                f"{self.config.to_str(sensitive_handling=True)}"
            )

            recorder = AgoraGenericRecorder(
                api_key=self.config.generic_video_api_key,
                app_id=self.config.agora_appid,
                app_cert=self.config.agora_appcert,
                channel_name=self.config.channel,
                avatar_uid=self.config.agora_avatar_uid,
                ten_env=ten_env,
                avatar_id=self.config.avatar_id,
                activity_idle_timeout=self.config.activity_idle_timeout,
                quality=self.config.quality,
                version=self.config.version,
                video_encoding=self.config.video_encoding,
                area=self.config.area,
                enable_string_uid=self.config.enable_string_uid,
                start_endpoint=self.config.start_endpoint,
                stop_endpoint=self.config.stop_endpoint,
                vendor_params=self.config.vendor_params,
            )

            self.recorder = recorder
            self._audio_processing_enabled = True

            self._audio_task = asyncio.create_task(
                self._loop_input_audio_sender(ten_env)
            )

            await self.recorder.connect()

        except ValueError as e:
            await self._handle_error(
                f"Config validation error: {e}",
                code=ERROR_CODE_CONFIG_VALIDATION_ERROR,
            )
        except Exception:
            ten_env.log_error(f"error on_start, {traceback.format_exc()}")

    async def _loop_input_audio_sender(self, _: AsyncTenEnv):
        while self._audio_processing_enabled:
            audio_frame, actual_sample_rate = await self.input_audio_queue.get()

            # Wait for recorder to be ready
            await self._wait_for_recorder_ready()

            if self.recorder is not None and self.recorder.ws_connected():
                try:
                    expected_rate = self.config.input_audio_sample_rate

                    if len(audio_frame) == 0:
                        continue

                    if (
                        expected_rate != actual_sample_rate
                        and not self._sample_rate_mismatch_warned
                    ):
                        self._sample_rate_mismatch_warned = True
                        self.ten_env.log_warn(
                            "Audio frame sample rate does not match configured "
                            f"input_audio_sample_rate: actual={actual_sample_rate}, "
                            f"configured={expected_rate}. "
                            "Sending the actual frame rate without resampling."
                        )

                    base64_audio_data = base64.b64encode(audio_frame).decode(
                        "utf-8"
                    )

                    await self.recorder.send(
                        base64_audio_data, sample_rate=actual_sample_rate
                    )

                except Exception as e:
                    self.ten_env.log_error(f"Error processing audio frame: {e}")
                    continue

    async def _wait_for_recorder_ready(self) -> None:
        """Wait for recorder to be ready and connected."""
        while self.recorder is None or not self.recorder.ws_connected():
            self.ten_env.log_debug("Recorder not ready, waiting...")
            await asyncio.sleep(0.5)

    async def _clear_audio_queue(self) -> None:
        """Clear audio queue before interrupt."""
        self.ten_env.log_info("Clearing audio queue before interrupt")
        # Clear all audio frames from the queue
        queue_size = self.input_audio_queue.qsize()
        cleared_count = 0
        for _ in range(queue_size):
            try:
                self.input_audio_queue.get_nowait()
                cleared_count += 1
            except asyncio.QueueEmpty:
                break
        self.ten_env.log_info(
            f"Cleared {cleared_count} audio frames from queue before interrupt"
        )

    async def _handle_interrupt(self) -> None:
        """Handle interrupt by clearing audio queue and sending interrupt command."""
        self.ten_env.log_info("Handling interrupt")
        await self._clear_audio_queue()

        # Send interrupt command
        if self.recorder and self.recorder.ws_connected():
            success = await self.recorder.interrupt()
            if success:
                self.ten_env.log_info(
                    "Successfully sent voice_interrupt command"
                )
            else:
                self.ten_env.log_error("Failed to send voice_interrupt command")

        self.ten_env.log_info("Interrupt handling completed")

    async def _handle_error(self, message: str, code: int = 0) -> None:
        """Handle and log errors consistently."""
        self.ten_env.log_error(f"Error {code}: {message}")

        # Send structured error message to system
        try:
            from ten_ai_base import ErrorMessage

            data = Data.create("message")
            error_msg = ErrorMessage(
                module="avatar",
                message=message,
                code=code,
            )
            data.set_property_from_json("", error_msg.model_dump_json())
            asyncio.create_task(self.ten_env.send_data(data))
        except ImportError:
            # Fall back to simple logging if ErrorMessage not available
            pass

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop")
        self._audio_processing_enabled = False

        # Cancel audio processing task
        if self._audio_task and not self._audio_task.done():
            self._audio_task.cancel()
            try:
                await self._audio_task
            except asyncio.CancelledError:
                pass

        # Cancel connection task if running
        if self._connection_task and not self._connection_task.done():
            self._connection_task.cancel()
            try:
                await self._connection_task
            except asyncio.CancelledError:
                pass

        if self.recorder:
            await self.recorder.disconnect()

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        if cmd_name == "flush":
            ten_env.log_debug(f"KEYPOINT [on_cmd:{cmd_name}]")
            await self._handle_interrupt()
            await ten_env.send_cmd(Cmd.create("flush"))

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

        if data_name == "tts_audio_end":
            import json

            json_str, _ = data.get_property_to_json(None)
            if json_str:
                payload = json.loads(json_str)
                reason = payload.get("reason")
                request_id = payload.get("request_id", "unknown")

                ten_env.log_info(
                    f"[GENERIC_TTS_END] Received tts_audio_end: "
                    f"request_id={request_id}, reason={reason}"
                )

                # reason=1 means TTS generation complete (all audio sent to avatar)
                if reason == 1:
                    ten_env.log_info(
                        "[GENERIC_TTS_END] TTS complete - sending voice_end"
                    )
                    if self.recorder and self.recorder.ws_connected():
                        await self.recorder.send_voice_end()
                    else:
                        ten_env.log_warn(
                            "[GENERIC_TTS_END] Recorder not ready, cannot send voice_end"
                        )

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        # Skip processing if audio processing is disabled
        if not self._audio_processing_enabled:
            return

        audio_frame_name = audio_frame.get_name()
        audio_frame_sample_rate = audio_frame.get_sample_rate()
        ten_env.log_debug(
            f"on_audio_frame name {audio_frame_name}, sample_rate {audio_frame_sample_rate}"
        )

        frame_buf = audio_frame.get_buf()
        if not frame_buf:
            self.ten_env.log_warn("on_audio_frame: empty pcm_frame detected.")
            return

        self.input_audio_queue.put_nowait(
            (frame_buf, audio_frame_sample_rate)
        )

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))
