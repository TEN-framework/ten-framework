import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.asr import ASRBufferConfigModeKeep
from ten_runtime import AudioFrame, Data
from websockets.protocol import State

from .. import recognition
from ..extension import XfyunDialectASRExtension


class ProviderSocket:
    def __init__(self):
        self.state = State.OPEN
        self.audio = bytearray()
        self.audio_received = asyncio.Event()
        self.live_audio_received = asyncio.Event()
        self.started = asyncio.Event()
        self.messages = asyncio.Queue()
        self.messages.put_nowait(
            json.dumps({"msg_type": "action", "data": {"action": "started"}})
        )

    def __aiter__(self):
        return self

    async def __anext__(self):
        message = await self.messages.get()
        if message is None:
            raise StopAsyncIteration
        self.started.set()
        return message

    async def send(self, message):
        if isinstance(message, bytes):
            self.audio.extend(message)
            self.audio_received.set()
            if b"\xff\x7f" * 16 in message:
                self.live_audio_received.set()
        else:
            assert json.loads(message) == {"end": True}
            await self.close()

    async def close(self):
        self.state = State.CLOSED
        self.messages.put_nowait(None)


def audio_frame(pcm):
    frame = AudioFrame.create("pcm_frame")
    frame.set_sample_rate(16000)
    frame.set_number_of_channels(1)
    frame.set_bytes_per_sample(2)
    frame.set_samples_per_channel(len(pcm) // 2)
    frame.set_property_from_json("metadata", '{"session_id": "100"}')
    frame.alloc_buf(len(pcm))
    buf = frame.lock_buf()
    buf[:] = pcm
    frame.unlock_buf(buf)
    return frame


async def finalize(extension, ten_env, finalize_id):
    data = Data.create("asr_finalize")
    data.set_property_string("finalize_id", finalize_id)
    await extension.on_data(ten_env, data)


def test_reconnect_preserves_audio_before_next_live_frame():
    async def scenario():
        sockets = [ProviderSocket(), ProviderSocket()]
        reconnect_waiting = asyncio.Event()
        allow_reconnect = asyncio.Event()
        connections = iter(sockets)

        async def connect(*_args, **_kwargs):
            socket = next(connections)
            if socket is sockets[1]:
                reconnect_waiting.set()
                await allow_reconnect.wait()
            return socket

        config = {
            "params": {
                "app_id": "test-app",
                "access_key_id": "test-key",
                "access_key_secret": "test-secret",
            }
        }
        ten_env = MagicMock()
        ten_env.get_property_bool = AsyncMock(return_value=(True, None))
        ten_env.get_property_to_json = AsyncMock(
            return_value=(json.dumps(config), None)
        )
        ten_env.send_data = AsyncMock()
        extension = XfyunDialectASRExtension("asr")
        buffer_config = extension.buffer_strategy()
        assert isinstance(buffer_config, ASRBufferConfigModeKeep)
        assert buffer_config.byte_limit == 1024 * 1024 * 10

        with patch.object(recognition.websockets, "connect", connect):
            await extension.on_init(ten_env)
            try:
                await extension.on_start(ten_env)
                await extension.on_audio_frame(
                    ten_env, audio_frame(b"\x01\x02" * 640)
                )
                await asyncio.wait_for(sockets[0].audio_received.wait(), 1)
                await finalize(extension, ten_env, "first")
                await asyncio.wait_for(reconnect_waiting.wait(), 1)

                buffered = [bytes([index, 0]) * 160 for index in range(142)]
                expected_buffered = b"".join(buffered)
                # The default keep buffer is capped by bytes, not duration.
                # All 1.42 seconds of audio fit within the 10 MiB limit.
                assert len(expected_buffered) < buffer_config.byte_limit
                for pcm in buffered:
                    await extension.on_audio_frame(ten_env, audio_frame(pcm))
                    # Let the audio consumer run while the handshake is held.
                    await asyncio.sleep(0)

                allow_reconnect.set()
                await asyncio.wait_for(sockets[1].started.wait(), 1)
                live_pcm = b"\xff\x7f" * 640
                await extension.on_audio_frame(ten_env, audio_frame(live_pcm))
                await asyncio.wait_for(sockets[1].live_audio_received.wait(), 1)
                await finalize(extension, ten_env, "second")

                assert sockets[1].audio == expected_buffered + live_pcm
            finally:
                await extension.on_stop(ten_env)
                await extension.on_deinit(ten_env)

    asyncio.run(scenario())
