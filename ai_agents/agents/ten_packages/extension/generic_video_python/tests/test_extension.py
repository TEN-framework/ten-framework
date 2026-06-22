#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

import asyncio
import json

from generic_video_python.extension import GenericVideoExtension


class FakeAudioFrame:
    def __init__(self, payload: bytes, sample_rate: int):
        self._payload = payload
        self._sample_rate = sample_rate

    def get_name(self) -> str:
        return "pcm"

    def get_sample_rate(self) -> int:
        return self._sample_rate

    def get_buf(self) -> bytes:
        return self._payload


class FakeData:
    def __init__(self, name: str, payload: dict):
        self._name = name
        self._payload = payload

    def get_name(self) -> str:
        return self._name

    def get_property_to_json(self, _path):
        return json.dumps(self._payload), None


class FakeEnv:
    def __init__(self):
        self.warns: list[str] = []
        self.infos: list[str] = []
        self.debugs: list[str] = []
        self.errors: list[str] = []
        self.sent_cmds = []

    def log_warn(self, msg: str, **_kwargs):
        self.warns.append(msg)

    def log_info(self, msg: str, **_kwargs):
        self.infos.append(msg)

    def log_debug(self, msg: str, **_kwargs):
        self.debugs.append(msg)

    def log_error(self, msg: str, **_kwargs):
        self.errors.append(msg)

    async def send_cmd(self, cmd):
        self.sent_cmds.append(cmd.get_name())

    async def return_result(self, _result):
        return None


class FakeRecorder:
    def __init__(self):
        self.voice_end_count = 0
        self.sent_audio: list[tuple[str, int]] = []

    def ws_connected(self) -> bool:
        return True

    async def send_voice_end(self):
        self.voice_end_count += 1

    async def send(self, audio_base64: str, sample_rate: int):
        self.sent_audio.append((audio_base64, sample_rate))

    async def interrupt(self):
        return True


def test_on_audio_frame_queues_actual_sample_rate():
    async def _run():
        extension = GenericVideoExtension("generic_video_python")
        extension.ten_env = FakeEnv()
        extension._audio_processing_enabled = True

        frame = FakeAudioFrame(b"\x01\x02", 44100)
        await extension.on_audio_frame(extension.ten_env, frame)

        payload, sample_rate = extension.input_audio_queue.get_nowait()
        assert payload == b"\x01\x02"
        assert sample_rate == 44100

    asyncio.run(_run())


def test_audio_sender_uses_actual_sample_rate_and_warns_once():
    async def _run():
        extension = GenericVideoExtension("generic_video_python")
        env = FakeEnv()
        extension.ten_env = env
        extension.config = type(
            "Config",
            (),
            {"input_audio_sample_rate": 16000},
        )()
        extension.recorder = FakeRecorder()
        extension._audio_processing_enabled = True

        await extension.input_audio_queue.put((b"\x00\x01", 48000))

        task = asyncio.create_task(extension._loop_input_audio_sender(env))
        await asyncio.sleep(0.05)
        extension._audio_processing_enabled = False
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

        assert extension.recorder.sent_audio[0][1] == 48000
        assert len(env.warns) == 1

    asyncio.run(_run())


def test_tts_audio_end_reason_one_triggers_voice_end():
    async def _run():
        extension = GenericVideoExtension("generic_video_python")
        extension.recorder = FakeRecorder()
        env = FakeEnv()

        await extension.on_data(
            env,
            FakeData(
                "tts_audio_end",
                {"reason": 1, "request_id": "req-1"},
            ),
        )

        assert extension.recorder.voice_end_count == 1

    asyncio.run(_run())
