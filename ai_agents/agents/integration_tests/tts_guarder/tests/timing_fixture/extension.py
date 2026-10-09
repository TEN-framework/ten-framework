#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""A test-only TEN producer emitting controlled valid and invalid TTS output."""

import asyncio
import json

import websockets
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    AudioFrame,
    AudioFrameDataFmt,
    Data,
)


class TimingFixtureExtension(AsyncExtension):
    def __init__(self, name):
        super().__init__(name)
        self.socket = None
        self.scenario = ""
        self.request_index = 0
        self.ready = asyncio.Event()

    async def on_start(self, ten_env: AsyncTenEnv):
        config_json, error = await ten_env.get_property_to_json("")
        assert error is None
        config = json.loads(config_json)
        self.scenario = config["scenario"]
        self.socket = await websockets.connect(config["url"])
        await self.socket.send(
            json.dumps(
                {
                    "event": "task_start",
                    "audio_setting": {"sample_rate": 16000, "channels": 1},
                }
            )
        )
        assert json.loads(await self.socket.recv())["event"] == "task_started"
        self.ready.set()

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        if data.get_name() != "tts_text_input":
            return
        await self.ready.wait()
        payload, error = data.get_property_to_json("")
        assert error is None
        request = json.loads(payload)
        request_index = self.request_index
        origin = 1000 + request_index * 4000
        self.request_index += 1
        metadata = dict(request["metadata"])
        metadata.update(turn_seq_id=0, turn_status=0)
        await self.send_result(ten_env, "tts_audio_start", request, metadata)
        await self.socket.send(
            json.dumps({"event": "task_continue", "text": request["text"]})
        )
        samples_before = 0
        while True:
            packet = json.loads(await self.socket.recv())
            if packet["event"] == "sentence_start":
                continue
            if packet["event"] == "task_flushed":
                break
            body = packet["data"]
            pcm = bytes.fromhex(body["audio"])
            timestamp = origin + samples_before * 1000 // 16000
            if self.scenario == "cumulative_drift":
                timestamp = origin + samples_before // 17113 * 1069
            await self.send_frame(ten_env, pcm, timestamp, metadata)
            samples_before += len(pcm) // 2
            if body.get("subtitle"):
                await self.send_caption(
                    ten_env, request, metadata, origin, body["subtitle"]
                )
            fractional = self.scenario in {
                "cumulative_drift",
                "fractional_clock",
            } or (self.scenario == "separate_origins" and request_index == 0)
            total = 17113 * 8 if fractional else 1600
            if samples_before == total:
                await self.socket.send(json.dumps({"event": "task_flush"}))
        terminal_metadata = {**metadata, "turn_seq_id": 1, "turn_status": 1}
        await self.send_result(
            ten_env,
            "tts_text_result",
            request,
            terminal_metadata,
            text="",
            words=[],
            start_ms=0,
            duration_ms=0,
            text_result_end=True,
        )
        await self.send_result(
            ten_env, "tts_audio_end", request, terminal_metadata, reason=1
        )

    @staticmethod
    async def send_result(ten_env, name, request, metadata, **values):
        data = Data.create(name)
        data.set_property_from_json(
            "",
            json.dumps(
                {
                    "request_id": request["request_id"],
                    "metadata": metadata,
                    **values,
                }
            ),
        )
        await ten_env.send_data(data)

    @staticmethod
    async def send_frame(ten_env, pcm, timestamp, metadata):
        frame = AudioFrame.create("pcm_frame")
        frame.set_timestamp(timestamp)
        frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
        frame.set_sample_rate(16000)
        frame.set_number_of_channels(1)
        frame.set_bytes_per_sample(2)
        frame.set_samples_per_channel(len(pcm) // 2)
        frame.set_property_from_json("metadata", json.dumps(metadata))
        frame.alloc_buf(len(pcm))
        buffer = frame.lock_buf()
        buffer[:] = pcm
        frame.unlock_buf(buffer)
        await ten_env.send_audio_frame(frame)

    async def send_caption(self, ten_env, request, metadata, origin, subtitle):
        raw = subtitle["timestamped_words"][0]
        offset = (
            0 if self.scenario == "erased_offset" else int(raw["time_begin"])
        )
        start = origin + offset
        await self.send_result(
            ten_env,
            "tts_text_result",
            request,
            metadata,
            text="word",
            start_ms=start,
            duration_ms=origin + int(subtitle["time_end"]) - start,
            words=[
                {
                    "word": "word",
                    "start_ms": start,
                    "duration_ms": int(raw["time_end"] - raw["time_begin"]),
                }
            ],
            text_result_end=False,
        )

    async def on_stop(self, _ten_env):
        if self.socket is not None:
            await self.socket.close()
