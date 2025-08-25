import asyncio
from typing import AsyncIterator
from pydantic import BaseModel, Field, ConfigDict
from groq import (
    AsyncGroq,
    APIConnectionError,
    APITimeoutError,
    RateLimitError,
    InternalServerError,
)

try:
    from .utils import encrypting_serializer, with_retry_context
    from .wav_stream_parser import WavStreamParser
except ImportError:
    from utils import encrypting_serializer, with_retry_context
    from wav_stream_parser import WavStreamParser


class GroqTTSParams(BaseModel):
    """
    GroqTTSParams
    https://console.groq.com/docs/text-to-speech
    https://console.groq.com/docs/api-reference#models
    """

    api_key: str = Field(..., description="the api key to use")
    model: str = Field("playai-tts", description="the model to use")
    voice: str = Field(..., description="the voice to use")
    response_format: str = Field(
        "wav", description="the format of the response"
    )
    sample_rate: int | None = Field(
        None, description="the sample rate of the response"
    )
    speed: float | None = Field(None, description="the speed of the response")

    _encrypt_fields = encrypting_serializer(
        "api_key",
    )
    model_config = ConfigDict(extra="allow")

    def to_request_params(self) -> dict:
        """
        convert the params to the params for the request
        """
        return self.model_dump(
            exclude_none=True,
            exclude={"api_key"},
        )


class GroqTTS:
    def __init__(
        self,
        params: GroqTTSParams,
        timeout: float = 30.0,
        max_retries: int = 3,
        retry_delay: float = 0.1,
    ):
        self.timeout = timeout
        self.max_retries = max_retries
        self.retry_delay = retry_delay

        self.params = params
        self.client: AsyncGroq | None = None

        try:
            self.client = AsyncGroq(api_key=params.api_key)
        except Exception as e:
            raise RuntimeError(
                f"error when initializing GroqTTS with params: {params.model_dump_json()}\nerror: {e}"
            ) from e

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        assert self.client is not None
        response = self.client.with_streaming_response.audio.speech.create(
            input=text, **self.params.to_request_params()
        )
        async with response as stream:
            stream_parser = WavStreamParser(stream.iter_bytes())
            _format_info = await stream_parser.get_format_info()
            async for chunk in stream_parser:
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
            exceptions=(
                APIConnectionError,
                APITimeoutError,
                RateLimitError,
                InternalServerError,
            ),
        )(self.synthesize)(text)
        async for chunk in response:
            yield chunk

    async def _is_valid_text(self, text: str) -> None:
        """check if the text is valid"""
        if len(text.strip()) == 0:
            raise ValueError("text is empty")


if __name__ == "__main__":
    import os
    import time

    params = GroqTTSParams(
        api_key=os.getenv("GROQ_API_KEY", ""),
        model="playai-tts",
        voice="Arista-PlayAI",
        response_format="wav",
        sample_rate=16000,
        speed=1.0,
    )
    print(params.model_dump_json())

    # test async
    async def test_async():
        tts = GroqTTS(params)
        t = time.time()
        print(f"start synthesize with retry - {t}")

        # test synthesize with retry
        f = open("test.pcm", "wb")
        try:
            async for chunk in tts.synthesize_with_retry("Hello, world!"):
                _len = len(chunk)
                print(
                    f"received {_len} delay: {time.time() - t} bytes: {chunk[:10]}..."
                )
                f.write(chunk)
        except Exception as e:
            print(f"error: {e}")
        f.close()

        n = 10
        while n > 0:
            assert tts.client is not None
            # pylint: disable=protected-access
            await tts.client._client.request("HEAD", "https://api.groq.com")
            await asyncio.sleep(1)
            n -= 1

        print("\n--- second test ---")
        t = time.time()
        async for chunk in tts.synthesize_with_retry("Hello, world!"):
            _len = len(chunk)
            print(
                f"received {_len} delay: {time.time() - t} bytes: {chunk[:10]}..."
            )

    asyncio.run(test_async())
