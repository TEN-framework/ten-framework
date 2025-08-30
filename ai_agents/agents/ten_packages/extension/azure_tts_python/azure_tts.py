import asyncio
import logging
import time
from functools import wraps
from typing import AsyncIterator, Iterator
from concurrent.futures import ThreadPoolExecutor
import azure.cognitiveservices.speech as speechsdk
from pydantic import BaseModel, Field, ConfigDict, field_validator

try:
    from .utils import encrypting_serializer
except ImportError:
    from utils import encrypting_serializer


class AzureTTSParams(BaseModel):
    """
    Speech_LogFilename: str
    """

    # pass to speechsdk.SpeechConfig
    subscription: str
    region: str | None = None
    endpoint: str | None = None
    host: str | None = None
    auth_token: str | None = None
    speech_recognition_language: str | None = None
    output_format: speechsdk.SpeechSynthesisOutputFormat | str | int = Field(
        default=speechsdk.SpeechSynthesisOutputFormat.Raw16Khz16BitMonoPcm,
        description="Output format",
    )

    propertys: list[tuple[speechsdk.PropertyId | str | int, str]] = Field(
        default_factory=list,
        description="Properties",
    )

    @field_validator("output_format", mode="before")
    @classmethod
    def validate_output_format(
        cls, value: str | speechsdk.SpeechSynthesisOutputFormat | int
    ) -> speechsdk.SpeechSynthesisOutputFormat:
        if isinstance(value, speechsdk.SpeechSynthesisOutputFormat):
            return value
        if isinstance(value, str):
            if hasattr(speechsdk.SpeechSynthesisOutputFormat, value):
                return getattr(speechsdk.SpeechSynthesisOutputFormat, value)
            else:
                raise ValueError(f"Invalid output format: {value}")
        if isinstance(value, int):
            return speechsdk.SpeechSynthesisOutputFormat(value)
        raise ValueError(f"Invalid output format: {value}")

    @field_validator("propertys", mode="before")
    @classmethod
    def validate_propertys(
        cls, value: list[tuple[speechsdk.PropertyId | str | int, str]]
    ) -> list[tuple[speechsdk.PropertyId, str]]:
        propertys = []
        for k, v in value:
            if not isinstance(v, str):
                raise ValueError(f"Invalid property value {k}:{v}")
            if isinstance(k, speechsdk.PropertyId):
                propertys.append((k, v))
            elif isinstance(k, str):
                if hasattr(speechsdk.PropertyId, k):
                    propertys.append((getattr(speechsdk.PropertyId, k), v))
                else:
                    raise ValueError(f"Invalid property key: {k}")
            elif isinstance(k, int):
                propertys.append((speechsdk.PropertyId(k), v))
            else:
                raise ValueError(f"Invalid property key: {k}")
        return propertys

    _encrypt_fields = encrypting_serializer(
        "subscription",
        "region",
        "auth_token",
    )
    model_config = ConfigDict(extra="allow")

    def to_speech_config_params(self) -> dict:
        """
        convert the params to the params for speechsdk.SpeechConfig
        """
        return self.model_dump(
            exclude_none=True,
            include=set(
                [
                    "subscription",
                    "region",
                    "endpoint",
                    "host",
                    "auth_token",
                    "speech_recognition_language",
                ]
            ),
        )


class AzureTTS:
    def __init__(self, params: AzureTTSParams, chunk_size: int = 3200):
        self.speech_synthesizer: speechsdk.SpeechSynthesizer | None = None
        self.chunk_size = chunk_size  # bytes
        self.is_connected = False
        self.thread_pool: ThreadPoolExecutor | None = None

        self.params = params

        try:
            self.speech_config = speechsdk.SpeechConfig(
                **params.to_speech_config_params()
            )
            assert isinstance(
                params.output_format, speechsdk.SpeechSynthesisOutputFormat
            )
            self.speech_config.set_speech_synthesis_output_format(
                params.output_format
            )
            for k, v in params.propertys:
                assert isinstance(k, speechsdk.PropertyId)
                self.speech_config.set_property(k, v)
        except Exception as e:
            raise RuntimeError(
                f"error when initializing AzureTTS with params: {params.model_dump_json()}\nerror: {e}"
            ) from e

        if not hasattr(asyncio, "to_thread"):
            self.thread_pool = ThreadPoolExecutor(max_workers=1)

    def sync_start_connection(
        self, pre_connect: bool = True, timeout: float = 30.0
    ):
        """
        start the connection to the speech service, and pre connect to the speech service if needed
        fully sync, will block the current thread
        """
        self.speech_synthesizer = speechsdk.SpeechSynthesizer(
            speech_config=self.speech_config, audio_config=None
        )
        # pre connect to the speech service, may be useful for the first time to connect to the speech service
        connection = speechsdk.Connection.from_speech_synthesizer(
            self.speech_synthesizer
        )
        connection.open(True)

        # pre connect to the speech service, may be take some time
        if pre_connect:
            try:
                _result = self.speech_synthesizer.start_speaking_text_async(
                    ""
                ).get()
                _stream = speechsdk.AudioDataStream(_result)

                start_time = time.time()
                while True:
                    if _stream.status == speechsdk.StreamStatus.AllData:
                        break
                    if _stream.status == speechsdk.StreamStatus.Canceled:
                        raise RuntimeError(
                            "connect to the speech service canceled by server"
                        )
                    time.sleep(0.1)
                    if timeout > 0 and time.time() - start_time > timeout:
                        raise TimeoutError(
                            "connect to the speech service timeout"
                        )
            except Exception as e:
                logging.error(
                    f"error when pre connecting to the speech service: {e}"
                )
                # clean up the connection
                self.sync_stop_connection()

        self.is_connected = self.speech_synthesizer is not None

        return self.is_connected

    def sync_stop_connection(self):
        """
        stop the connection to the speech service
        """
        if self.speech_synthesizer is None:
            self.is_connected = False
            return

        try:
            self.speech_synthesizer.stop_speaking()
        except Exception:
            ...
        self.speech_synthesizer = None
        self.is_connected = False

    def sync_synthesize(self, text: str) -> Iterator[bytes]:
        """
        synthesize the text, return the iterator of audio chunks or raise error if failed
        """
        if not self.is_connected:
            raise RuntimeError("not connected to the speech service")

        # synthesize the text
        assert self.speech_synthesizer is not None
        speech_synthesis_result = self.speech_synthesizer.start_speaking_text(
            text
        )
        stream = speechsdk.AudioDataStream(speech_synthesis_result)

        while True:
            buffer = bytes(self.chunk_size)
            filled_size = stream.read_data(buffer)
            if filled_size == 0:
                break
            yield buffer[:filled_size]

    def sync_synthesize_ssml(self, ssml: str) -> Iterator[bytes]:
        """
        synthesize the ssml, return the iterator of audio chunks or raise error if failed
        """
        if not self.is_connected:
            raise RuntimeError("not connected to the speech service")

        # synthesize the text
        assert self.speech_synthesizer is not None
        speech_synthesis_result = self.speech_synthesizer.start_speaking_ssml(
            ssml
        )
        stream = speechsdk.AudioDataStream(speech_synthesis_result)

        while True:
            buffer = bytes(self.chunk_size)
            filled_size = stream.read_data(buffer)
            if filled_size == 0:
                break
            yield buffer[:filled_size]

    async def _async_iter_from_sync(
        self, sync_iterator: Iterator[bytes]
    ) -> AsyncIterator[bytes]:
        """
        convert the sync iterator to an async iterator, support cancel operation
        """
        try:
            for chunk in sync_iterator:
                # check if the task is cancelled
                current_task = asyncio.current_task()
                if current_task and current_task.cancelled():
                    logging.info("task is cancelled")
                    break

                # returnt the control to the event loop
                await asyncio.sleep(0)
                yield chunk
        except Exception as e:
            logging.error(f"error when iterating audio stream: {e}")
            raise

    def _wrap_sync_func(self, sync_func, timeout: float | None = 30.0):
        """
        wrap the sync function to a async function, support timeout
        """

        async def _async_func(*args, **kwargs):
            # use asyncio.to_thread instead of run_in_executor (Python 3.9+)
            if hasattr(asyncio, "to_thread"):
                result = await asyncio.wait_for(
                    asyncio.to_thread(sync_func, *args, **kwargs),
                    timeout=timeout,
                )
            else:
                assert self.thread_pool is not None
                loop = asyncio.get_event_loop()
                result = await asyncio.wait_for(
                    loop.run_in_executor(
                        self.thread_pool, sync_func, *args, **kwargs
                    ),
                    timeout=timeout,
                )
            return result

        return _async_func

    async def synthesize(self, *args, **kwargs) -> AsyncIterator[bytes]:
        it = await self._wrap_sync_func(self.sync_synthesize, timeout=None)(
            *args, **kwargs
        )
        return self._async_iter_from_sync(it)

    async def synthesize_ssml(self, *args, **kwargs) -> AsyncIterator[bytes]:
        it = await self._wrap_sync_func(
            self.sync_synthesize_ssml, timeout=None
        )(*args, **kwargs)
        return self._async_iter_from_sync(it)

    async def start_connection(self, *args, **kwargs):
        return await self._wrap_sync_func(
            self.sync_start_connection, timeout=30.0
        )(*args, **kwargs)

    async def stop_connection(self, *args, **kwargs):
        return await self._wrap_sync_func(
            self.sync_stop_connection, timeout=30.0
        )(*args, **kwargs)

    def _wrap_retry(self, max_retries: int = 3, retry_delay: float = 1.0):
        """
        wrap the function to a retry function
        """

        def _wrap_retry(func):
            @wraps(func)
            async def _wrapper(*args, **kwargs):
                current_retry_delay = retry_delay
                for _ in range(max_retries):
                    if not self.is_connected:
                        await self.start_connection()
                    try:
                        return await func(*args, **kwargs)
                    except Exception as e:
                        logging.error(
                            f"error when calling {func.__name__}: {e}"
                        )
                        await self.stop_connection()
                        await asyncio.sleep(current_retry_delay)
                        current_retry_delay *= 2

                raise RuntimeError(
                    f"failed to call {func.__name__} after {max_retries} retries"
                )

            return _wrapper

        return _wrap_retry

    async def synthesize_with_retry(
        self, text: str, max_retries: int = 3, retry_delay: float = 1.0
    ) -> AsyncIterator[bytes]:
        return await self._wrap_retry(max_retries, retry_delay)(
            self.synthesize
        )(text)

    async def synthesize_ssml_with_retry(
        self, ssml: str, max_retries: int = 3, retry_delay: float = 1.0
    ) -> AsyncIterator[bytes]:
        return await self._wrap_retry(max_retries, retry_delay)(
            self.synthesize_ssml
        )(ssml)

    def __del__(self):
        self.sync_stop_connection()


if __name__ == "__main__":
    import os

    params = AzureTTSParams(
        subscription=os.getenv("AZURE_TTS_API_KEY", ""),
        region=os.getenv("AZURE_TTS_REGION", ""),
        output_format="Raw16Khz16BitMonoPcm",
        propertys=[
            ("Speech_LogFilename", "azure_tts_log.txt"),
            ("SpeechServiceConnection_SynthLanguage", "en-US"),
            ("SpeechServiceConnection_SynthVoice", "en-US-AriaNeural"),
        ],
    )
    print(params.model_dump_json())
    tts = AzureTTS(params)
    tts.sync_start_connection(pre_connect=True)
    print("start synthesize")
    f = open("test.pcm", "wb")
    t = time.time()
    for chunk in tts.sync_synthesize("I'm excited to be here today!"):
        _len = len(chunk)
        f.write(chunk)
        print(
            f"received {_len} bytes delay: {time.time() - t}: {chunk[:10]}..."
        )
    f.close()
    tts.sync_stop_connection()

    input("press enter to test async")

    # test async
    async def test_async():
        tts = AzureTTS(params)
        await tts.start_connection(pre_connect=True)
        t = time.time()
        print("start synthesize")
        async for chunk in await tts.synthesize_with_retry("Hello, world!"):
            _len = len(chunk)
            print(
                f"received {_len} bytes delay: {time.time() - t}s: {chunk[:10]}..."
            )
        await tts.stop_connection()

    asyncio.run(test_async())
