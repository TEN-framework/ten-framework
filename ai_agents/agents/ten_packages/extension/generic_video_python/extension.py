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
from .generic import AgoraGenericRecorder
from dataclasses import dataclass


@dataclass
class GenericVideoConfig(BaseConfig):
    agora_appid: str = ""
    agora_appcert: str = ""
    agora_channel_name: str = ""
    agora_video_uid: int = 0
    generic_video_api_key: str = ""
    start_endpoint: str = "https://api.example.com/v1/sessions/start"
    stop_endpoint: str = "https://api.example.com/v1/sessions/stop"
    input_audio_sample_rate: int = 48000


class GenericVideoExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config = None
        self.input_audio_queue = asyncio.Queue()
        self.audio_queue = asyncio.Queue[bytes]()
        self.video_queue = asyncio.Queue()
        self.recorder: AgoraGenericRecorder = None
        self.ten_env: AsyncTenEnv = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        try:
            self.config = await GenericVideoConfig.create_async(ten_env)

            recorder = AgoraGenericRecorder(
                api_key=self.config.generic_video_api_key,
                app_id=self.config.agora_appid,
                app_cert=self.config.agora_appcert,
                channel_name=self.config.agora_channel_name,
                avatar_uid=self.config.agora_video_uid,
                ten_env=ten_env,
                start_endpoint=self.config.start_endpoint,
                stop_endpoint=self.config.stop_endpoint,
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
                try:
                    original_rate = self.config.input_audio_sample_rate
                    target_rate = 24000

                    audio_data = np.frombuffer(audio_frame, dtype=np.int16)
                    if len(audio_data) == 0:
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
                            old_indices = np.linspace(0, len(audio_data) - 1, new_length)
                            resampled_audio = audio_data[np.round(old_indices).astype(int)]
                        else:
                            # Downsampling: select fewer samples
                            step = 1 / resample_ratio
                            indices = np.round(np.arange(0, len(audio_data), step)).astype(int)
                            indices = indices[indices < len(audio_data)]
                            resampled_audio = audio_data[indices]

                        resampled_frame = resampled_audio.tobytes()

                    # Encode and send
                    base64_audio_data = base64.b64encode(resampled_frame).decode("utf-8")
                    await self.recorder.send(base64_audio_data)

                except Exception as e:
                    self.ten_env.log_error(f"Error processing audio frame: {e}")
                    continue

    def _dump_audio_if_need(self, buf: bytearray) -> None:
        with open(
            "{}_{}.pcm".format("tts", self.config.agora_channel_name), "ab"
        ) as dump_file:
            dump_file.write(buf)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop")
        if self.recorder:
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