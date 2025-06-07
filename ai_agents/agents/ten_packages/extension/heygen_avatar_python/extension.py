#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import base64
import traceback
import numpy as np
from scipy.signal import resample_poly

from ten import (
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
# from .heygen_bak import HeyGenRecorder
from dataclasses import dataclass


@dataclass
class HeygenAvatarConfig(BaseConfig):
    agora_appid: str = ""
    agora_appcert: str = ""
    agora_channel_name: str = ""
    agora_avatar_uid: int = 0
    heygen_api_key: str = ""
    input_audio_sample_rate: int = 48000


class HeygenAvatarExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config = None
        self.input_audio_queue = asyncio.Queue()
        self.audio_queue = asyncio.Queue[bytes]()
        self.video_queue = asyncio.Queue()
        self.recorder: AgoraHeygenRecorder = None
        self.ten_env: AsyncTenEnv = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        try:
            self.config = await HeygenAvatarConfig.create_async(ten_env)

            # recorder = HeyGenRecorder(
            #     self.config.api_key,
            #     self.config.avatar_name,
            #     ten_env=ten_env,
            #     audio_queue=self.audio_queue,
            #     video_queue=self.video_queue,
            # )

            recorder = AgoraHeygenRecorder(
                heygen_api_key=self.config.heygen_api_key,
                app_id=self.config.agora_appid,
                app_cert=self.config.agora_appcert,
                channel_name=self.config.agora_channel_name,
                avatar_uid=self.config.agora_avatar_uid,
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
            if self.recorder is not None and self.recorder.ws_connected():
                # Downsample the audio before sending
                try:
                    # Assume audio_frame contains PCM audio at the original sample rate
                    original_rate = self.config.input_audio_sample_rate  # Use the configured sample rate
                    target_rate = 24000

                    # Dump if needed
                    self._dump_audio_if_need(audio_frame)

                    audio_data = np.frombuffer(audio_frame, dtype=np.int16)
                    if len(audio_data) == 0:
                        continue


                    # Calculate up/down factors for rational resampling
                    gcd = np.gcd(original_rate, target_rate)
                    up = target_rate // gcd
                    down = original_rate // gcd

                    # self.ten_env.log_info(
                    #     f"Resampling audio from {original_rate}Hz to {target_rate}Hz with up={up}, down={down}")

                    # Apply resampling (polyphase filtering)
                    resampled = resample_poly(audio_data, up=up, down=down)
                    resampled = np.clip(resampled, -32768, 32767).astype(np.int16)
                    resampled_bytes = resampled.tobytes()


                    # Encode and send
                    base64_audio_data = base64.b64encode(resampled_bytes).decode("utf-8")
                    await self.recorder.send(base64_audio_data)

                except Exception as e:
                    # Log error but continue processing
                    self.ten_env.log_error(f"Error processing audio frame: {e}")
                    continue

    def _dump_audio_if_need(self, buf: bytearray) -> None:
        with open(
            "{}_{}.pcm".format("tts", self.config.agora_channel_name), "ab"
        ) as dump_file:
            dump_file.write(buf)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
        await self.recorder.disconnect()
        # TODO: clean up resources

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        # TODO: process cmd

        cmd_result = CmdResult.create(StatusCode.OK)
        await ten_env.return_result(cmd_result, cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug("on_audio_frame name {}".format(audio_frame_name))

        frame_buf = audio_frame.get_buf()
        self.input_audio_queue.put_nowait(frame_buf)

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))
