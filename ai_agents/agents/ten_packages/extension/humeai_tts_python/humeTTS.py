import asyncio
import base64
from typing import AsyncIterator

# Only import the specific TTS modules we need to avoid PortAudio dependency
from hume import AsyncHumeClient
from hume.tts import (
    FormatPcm,
    PostedContextWithGenerationId,
    PostedUtterance,
    PostedUtteranceVoiceWithId,
    PostedUtteranceVoiceWithName,
)
from ten_runtime import AsyncTenEnv
from .config import HumeAiTTSConfig

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3

class HumeAiTTS:
    def __init__(self, config: HumeAiTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.connection = AsyncHumeClient(api_key=config.key)
        self.generation_id = config.generation_id
        self.generation_id_lock = asyncio.Lock()
        self._is_cancelled = False

    async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int]]:
        self._is_cancelled = False

        self.ten_env.log_debug(
            f"KEYPOINT generate_TTS for '{text}' "
            f"with generation_id {self.generation_id}"
        )

        context = None
        async with self.generation_id_lock:
            if self.generation_id:
                context = PostedContextWithGenerationId(generation_id=self.generation_id)

        voice = None
        if self.config.voice_name:
            voice = PostedUtteranceVoiceWithName(name=self.config.voice_name, provider=self.config.provider)
        elif self.config.voice_id:
            voice = PostedUtteranceVoiceWithId(id=self.config.voice_id, provider=self.config.provider)

        try:
            async for snippet in self.connection.tts.synthesize_json_streaming(
                context=context,
                utterances=[
                    PostedUtterance(
                        text=text,
                        voice=voice,
                        speed=self.config.speed,
                        trailing_silence=self.config.trailing_silence,
                    )
                ],
                format=FormatPcm(type="pcm"),
                instant_mode=True,
            ):
                if self._is_cancelled:
                    self.ten_env.log_info("Cancellation flag detected, stopping TTS stream.")
                    break

                async with self.generation_id_lock:
                    self.generation_id = snippet.generation_id

                audio_bytes = base64.b64decode(snippet.audio)
                yield audio_bytes, EVENT_TTS_RESPONSE

                if snippet.is_last_chunk:
                    break

            yield None, EVENT_TTS_END

        except Exception as e:
            self.ten_env.log_error(f"Hume TTS streaming failed: {e}")
            yield str(e).encode('utf-8'), EVENT_TTS_ERROR

    async def cancel(self):
        self.ten_env.log_debug("HumeAiTTS: cancel() called.")
        self._is_cancelled = True

    def clean(self):
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("HumeAiTTS: clean() called.")
