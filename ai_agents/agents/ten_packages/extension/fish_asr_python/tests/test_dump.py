"""Regression coverage for raw PCM dumping and writer cleanup."""

import asyncio
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.asr import AsyncASRBaseExtension

from ..extension import FishAudioASRExtension


async def initialize_extension(dump_path, *, dump=True):
    env = MagicMock()
    env.get_property_to_json = AsyncMock(
        return_value=(
            json.dumps(
                {
                    "dump": dump,
                    "dump_path": str(dump_path),
                    "params": {"api_key": "test-key"},
                }
            ),
            None,
        )
    )
    env.send_data = AsyncMock()
    extension = FishAudioASRExtension("fish_asr_python")
    extension.ten_env = env
    # Exercise the extension lifecycle without starting the base audio consumer.
    with patch.object(AsyncASRBaseExtension, "on_init", new=AsyncMock()):
        await extension.on_init(env)
    extension.client = SimpleNamespace(
        connected=True,
        send_audio=AsyncMock(),
        close=AsyncMock(),
        session_done=asyncio.Event(),
    )
    return extension, env


def audio_frame(data):
    return SimpleNamespace(get_buf=lambda: data)


def test_dump_preserves_pcm_across_connections_and_flushes_on_stop(tmp_path):
    async def run():
        extension, env = await initialize_extension(tmp_path / "audio")
        first_audio = b"\x01\x02" * 160
        second_audio = b"\x03\x04" * 320
        dumper = extension.audio_dumper
        try:
            assert await extension.send_audio(audio_frame(first_audio), None)
            await extension.stop_connection()
            assert extension.audio_dumper is dumper
            extension.client = SimpleNamespace(
                connected=True, send_audio=AsyncMock(), close=AsyncMock()
            )
            assert await extension.send_audio(audio_frame(second_audio), None)
            await extension.on_stop(env)
            assert extension.audio_dumper is None
            assert (tmp_path / "audio" / "fish_asr_in.pcm").read_bytes() == (
                first_audio + second_audio
            )
        finally:
            await extension.on_deinit(env)

    asyncio.run(run())


def test_dump_disabled_creates_no_file(tmp_path):
    async def run():
        dump_path = tmp_path / "disabled"
        extension, env = await initialize_extension(dump_path, dump=False)
        try:
            assert await extension.send_audio(audio_frame(b"\x01\x02"), None)
            assert extension.audio_dumper is None
            assert not dump_path.exists()
        finally:
            await extension.on_stop(env)
            await extension.on_deinit(env)

    asyncio.run(run())


def test_dump_excludes_audio_that_was_not_sent(tmp_path):
    async def run():
        extension, env = await initialize_extension(tmp_path)
        try:
            extension.client.connected = False
            assert not await extension.send_audio(
                audio_frame(b"\x01\x02"), None
            )
            extension.client.connected = True
            extension.client.send_audio.side_effect = ConnectionError("closed")
            assert not await extension.send_audio(
                audio_frame(b"\x03\x04"), None
            )
            await extension.on_stop(env)
            assert (tmp_path / "fish_asr_in.pcm").read_bytes() == b""
        finally:
            await extension.on_deinit(env)

    asyncio.run(run())


def test_deinit_closes_dump_when_stop_was_not_called(tmp_path):
    async def run():
        extension, env = await initialize_extension(tmp_path)
        audio = b"\x01\x02" * 160
        assert await extension.send_audio(audio_frame(audio), None)
        await extension.on_deinit(env)
        await extension.on_deinit(env)
        assert extension.audio_dumper is None
        assert (tmp_path / "fish_asr_in.pcm").read_bytes() == audio

    asyncio.run(run())
