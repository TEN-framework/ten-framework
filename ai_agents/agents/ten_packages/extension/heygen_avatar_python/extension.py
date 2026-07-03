#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import base64
import traceback
import numpy as np

from ten_runtime import (  # pylint: disable=import-error
    AudioFrame,
    VideoFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from ten_ai_base.config import BaseConfig
from .heygen import AgoraHeygenRecorder
from dataclasses import dataclass


@dataclass
class HeygenAvatarConfig(BaseConfig):
    agora_appid: str = ""
    agora_appcert: str = ""
    channel: str = ""
    agora_avatar_uid: int = 0
    heygen_api_key: str = ""
    input_audio_sample_rate: int = 48000
    avatar_name: str = "Wayne_20240711"


class HeygenAvatarExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config = None
        self.input_audio_queue = asyncio.Queue()
        self.recorder: AgoraHeygenRecorder = None
        self.ten_env: AsyncTenEnv = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        try:
            self.config = await HeygenAvatarConfig.create_async(ten_env)
            ten_env.log_info(
                f"[AVATAR CONFIG] avatar_name={self.config.avatar_name}, channel={self.config.channel}"
            )

            recorder = AgoraHeygenRecorder(
                heygen_api_key=self.config.heygen_api_key,
                app_id=self.config.agora_appid,
                app_cert=self.config.agora_appcert,
                channel_name=self.config.channel,
                avatar_uid=self.config.agora_avatar_uid,
                avatar_name=self.config.avatar_name,
                ten_env=ten_env,
            )

            self.recorder = recorder

            asyncio.create_task(self._loop_input_audio_sender(ten_env))

            await self.recorder.connect()
        except Exception:
            ten_env.log_error(f"error on_start, {traceback.format_exc()}")

    async def _loop_input_audio_sender(self, _: AsyncTenEnv):
        while True:
            audio_frame = await self.input_audio_queue.get()

            if self.recorder is None:
                self.ten_env.log_warn("Recorder is None, dropping audio")
                continue

            if not self.recorder.ws_connected():
                self.ten_env.log_warn("WebSocket not connected, dropping audio")
                continue

            try:
                original_rate = self.config.input_audio_sample_rate
                target_rate = 24000

                audio_data = np.frombuffer(audio_frame, dtype=np.int16)
                if len(audio_data) == 0:
                    self.ten_env.log_warn("Empty audio data")
                    continue

                # Skip resampling if rates are the same
                if original_rate == target_rate:
                    resampled_frame = audio_frame
                else:
                    # Calculate resampling ratio
                    resample_ratio = target_rate / original_rate

                    if resample_ratio > 1:
                        # Upsampling: create more samples
                        new_length = int(len(audio_data) * resample_ratio)
                        old_indices = np.linspace(
                            0, len(audio_data) - 1, new_length
                        )
                        resampled_audio = audio_data[
                            np.round(old_indices).astype(int)
                        ]
                    else:
                        # Downsampling: select fewer samples
                        step = 1 / resample_ratio
                        indices = np.round(
                            np.arange(0, len(audio_data), step)
                        ).astype(int)
                        indices = indices[indices < len(audio_data)]
                        resampled_audio = audio_data[indices]

                    resampled_frame = resampled_audio.tobytes()

                # Encode and send
                base64_audio_data = base64.b64encode(resampled_frame).decode(
                    "utf-8"
                )
                await self.recorder.send(base64_audio_data)

            except Exception as e:
                self.ten_env.log_error(f"Error processing audio frame: {e}")
                continue

    async def _handle_interrupt(self) -> None:
        """Handle audio interrupt by clearing the audio queue and interrupting the client."""
        self.ten_env.log_debug("Handling interrupt")
        await self._clear_audio_queue()
        if self.recorder and self.recorder.ws_connected():
            await self.recorder.interrupt()
        else:
            self.ten_env.log_warn("Recorder not available or not connected")

    async def _clear_audio_queue(self) -> None:
        """Clear audio queue before interrupt."""
        queue_size = self.input_audio_queue.qsize()
        cleared_count = 0
        for _ in range(queue_size):
            try:
                self.input_audio_queue.get_nowait()
                cleared_count += 1
            except asyncio.QueueEmpty:
                break
        if cleared_count > 0:
            self.ten_env.log_debug(
                f"Cleared {cleared_count} audio frames from queue"
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.recorder:
            await self.recorder.disconnect()
            ten_env.log_info("[AVATAR] Disconnected recorder")
        else:
            ten_env.log_warn("[AVATAR] No recorder to disconnect")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        if cmd_name == "flush":
            ten_env.log_debug("Handling flush command")
            await self._handle_interrupt()
            # Send flush command downstream
            await ten_env.send_cmd(Cmd.create("flush"))

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

        if data_name == "tts_audio_end":
            # Parse tts_audio_end payload
            import json

            json_str, _ = data.get_property_to_json(None)
            if json_str:
                payload = json.loads(json_str)
                reason = payload.get("reason")
                request_id = payload.get("request_id", "unknown")

                ten_env.log_info(
                    f"[AVATAR_TTS_END] Received tts_audio_end: request_id={request_id}, reason={reason}"
                )

                # reason=1 means TTS generation complete (all audio sent to avatar)
                if reason == 1:
                    ten_env.log_info(
                        "[AVATAR_TTS_END] TTS generation complete (reason=1) - sending agent.speak_end to HeyGen"
                    )
                    if self.recorder:
                        await self.recorder.send_speak_end()
                    else:
                        ten_env.log_warn(
                            "[AVATAR_TTS_END] Recorder not initialized, cannot send speak_end"
                        )

    async def on_audio_frame(
        self, _ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        frame_buf = audio_frame.get_buf()
        self.input_audio_queue.put_nowait(frame_buf)

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))
