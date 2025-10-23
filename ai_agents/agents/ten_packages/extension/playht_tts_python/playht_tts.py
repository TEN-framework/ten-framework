from pydantic import BaseModel, Field, ConfigDict, field_validator
from typing import AsyncIterator

from pyht.client import TTSOptions, Language, Format
from pyht import AsyncClient

try:
    from .utils import encrypting_serializer, with_retry_context
except ImportError:
    from utils import encrypting_serializer, with_retry_context


class PlayHTParams(BaseModel):
    """
    PlayHTParams
    https://docs.play.ht/reference/python-sdk
    """

    api_key: str = Field(..., description="the api key to use")
    user_id: str = Field(..., description="the user id to use")
    voice_engine: str | None = Field(
        None, description="The voice engine to use for the TTS request."
    )
    protocol: str = Field(
        "ws", description="The protocol to use for the TTS request."
    )
    _encrypt_fields = encrypting_serializer(
        "api_key",
        "user_id",
    )
    #  for TTSOptions
    model_config = ConfigDict(extra="allow")
    language: Language | None = Field(
        None, description="The language to use for the TTS request."
    )
    format: Format = Field(
        Format.FORMAT_PCM, description="The format to use for the TTS request."
    )
    sample_rate: int = Field(
        16000, description="The sample rate to use for the TTS request."
    )

    @field_validator("format", mode="before")
    @classmethod
    def validate_format(cls, value: str | Format) -> Format:
        if isinstance(value, Format):
            return value
        if isinstance(value, str) and hasattr(Format, value):
            return getattr(Format, value)
        raise ValueError(f"Invalid format: {value}")

    @field_validator("language", mode="before")
    @classmethod
    def validate_language(cls, value: str | Language) -> Language:
        if isinstance(value, Language):
            return value
        if isinstance(value, str) and hasattr(Language, value):
            return getattr(Language, value)
        raise ValueError(f"Invalid language: {value}")

    def to_tts_options(self) -> TTSOptions:
        """
        convert the params to the params for the request
        """
        return TTSOptions(
            **self.model_dump(
                exclude_none=True,
                exclude={"api_key", "user_id", "voice_engine", "protocol"},
            )
        )


class PlayHTTTS:
    def __init__(
        self,
        params: PlayHTParams,
        timeout: float = 30.0,
        max_retries: int = 2,
        retry_delay: float = 0.05,
    ):
        self.timeout = timeout
        self.max_retries = max_retries
        self.retry_delay = retry_delay

        self.params = params
        self.client: AsyncClient | None = None

        try:
            self.client = AsyncClient(
                api_key=params.api_key, user_id=params.user_id
            )
        except Exception as e:
            raise RuntimeError(
                f"error when initializing PlayHTTTS with params: {params.model_dump_json()}\nerror: {e}"
            ) from e

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        assert self.client is not None
        await self._is_valid_text(text)
        async for chunk in self.client.tts(
            text,
            self.params.to_tts_options(),
            voice_engine=self.params.voice_engine,
            protocol=self.params.protocol,
        ):
            yield chunk

    async def synthesize_with_retry(self, text: str) -> AsyncIterator[bytes]:
        """synthesize with retry"""
        assert self.client is not None
        if len(text.strip()) == 0:
            raise ValueError("text is empty")
        response = with_retry_context(
            max_retries=self.max_retries,
            retry_delay=self.retry_delay,
            backoff_factor=2.0,
            exceptions=(Exception,),
        )(self.synthesize)(text)
        async for chunk in response:
            yield chunk

    async def _is_valid_text(self, text: str) -> None:
        """check if the text is valid"""
        if len(text.strip()) == 0:
            raise ValueError("text is empty")

    def close(self):
        # release client, playht sdk bug, when release the client, sdk will raise an error
        # but we can ignore it
        try:
            self.client = None
        except Exception:
            ...


if __name__ == "__main__":
    import os
    import time
    import asyncio

    playht_params = PlayHTParams(
        api_key=os.getenv("PLAYHT_TTS_SECRET_KEY", ""),
        user_id=os.getenv("PLAYHT_TTS_USER_ID", ""),
        voice_engine="PlayDialog",
        voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
    )

    print(playht_params.model_dump_json())

    # test async
    async def test_async():
        tts = PlayHTTTS(playht_params)
        t = time.time()
        print(f"start synthesize with retry - {t}")

        # test synthesize with retry
        f = open("test.pcm", "wb")
        try:
            async for chunk in tts.synthesize_with_retry("你好"):
                _len = len(chunk)
                print(
                    f"received {_len} delay: {time.time() - t} bytes: {chunk[:10]}..."
                )
                f.write(chunk)
        except Exception as e:
            print(f"error: {e}")
        f.close()

        print("\n--- second test ---")
        t = time.time()
        async for chunk in tts.synthesize_with_retry("Hello, world!"):
            _len = len(chunk)
            print(
                f"received {_len} delay: {time.time() - t} bytes: {chunk[:10]}..."
            )

        tts.close()

    asyncio.run(test_async())
