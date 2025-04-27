#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import time
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
    AudioFrameDataFmt,
)
from pydub import AudioSegment


class DefaultExtension(Extension):
    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")
        self.audio = AudioSegment.from_file(
            "../test_data/speech_16k_1.pcm",
            format="s16le",
            sample_width=2,
            channels=1,
            frame_rate=16000,
        )
        ten_env.log_info(f"audio duration: {len(self.audio)}")

        self.start_time = int(time.time() * 1000)
        ten_env.on_init_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info(f"DefaultExtension on_cmd json: {cmd_json}")

        self.request_cmd = cmd

        raw_data = self.audio.raw_data
        ten_env.log_info(f"audio raw data len: {len(raw_data)}")

        pcm_data = self.audio[0:10].raw_data
        audio_frame = AudioFrame.create("pcm")
        audio_frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
        audio_frame.set_bytes_per_sample(2)
        audio_frame.set_sample_rate(16000)
        audio_frame.set_number_of_channels(1)
        audio_frame.set_samples_per_channel(160)

        timestamp = int(time.time() * 1000)
        audio_frame.set_timestamp(timestamp)

        audio_frame.alloc_buf(len(pcm_data))
        buf = audio_frame.lock_buf()
        buf[: len(pcm_data)] = pcm_data
        audio_frame.unlock_buf(buf)
        ten_env.send_audio_frame(audio_frame)

    def on_audio_frame(self, ten_env: TenEnv, audio_frame: AudioFrame) -> None:
        ten_env.log_info("DefaultExtension on_audio_frame")

        assert audio_frame.get_bytes_per_sample() == 2
        assert audio_frame.get_sample_rate() == 16000
        assert audio_frame.get_number_of_channels() == 1
        assert audio_frame.get_samples_per_channel() == 160

        timestamp = audio_frame.get_timestamp()

        current_time = int(time.time() * 1000)
        assert current_time >= timestamp >= self.start_time

        buf = audio_frame.get_buf()

        assert buf[: len(buf)] == self.audio[0:10].raw_data

        cmd_result = CmdResult.create(StatusCode.OK, self.request_cmd)
        cmd_result.set_property_string("detail", "success")
        ten_env.return_result(cmd_result)

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log_info("DefaultExtension on_stop")
        ten_env.on_stop_done()
