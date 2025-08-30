import asyncio
import logging
import traceback
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from typing import AsyncIterator, Iterator

import boto3
from botocore.config import Config
from botocore.exceptions import ClientError
from pydantic import BaseModel, ConfigDict, Field

try:
    from .utils import encrypting_serializer
except ImportError:
    from utils import encrypting_serializer


class PollyTTSParams(BaseModel):
    # for aws credentials
    aws_access_key_id: str
    aws_secret_access_key: str
    aws_session_token: str | None = None
    region_name: str | None = None
    profile_name: str | None = None
    aws_account_id: str | None = None

    # for speech synthesis
    engine: str = Field(
        default="neural",
        description="Engine to use for speech synthesis",
        alias="Engine",
    )
    voice: str = Field(
        default="Joanna",
        description="Voice to use for speech synthesis",
        alias="VoiceId",
    )
    sample_rate: str = Field(
        default="16000",
        description="Sample rate to use for speech synthesis",
        alias="SampleRate",
    )
    lang_code: str = Field(
        default="en-US",
        description="Language code to use for speech synthesis",
        alias="LanguageCode",
    )
    audio_format: str = Field(
        default="pcm",
        description="Audio format to use for speech synthesis",
        alias="OutputFormat",
    )

    model_config = ConfigDict(extra="allow", populate_by_name=True)
    _encrypt_fields = encrypting_serializer(
        "aws_access_key_id", "aws_secret_access_key", "aws_session_token"
    )

    def to_session_params(self) -> dict:
        return {
            "aws_access_key_id": self.aws_access_key_id,
            "aws_secret_access_key": self.aws_secret_access_key,
            "aws_session_token": self.aws_session_token,
            "region_name": self.region_name,
            "profile_name": self.profile_name,
            "aws_account_id": self.aws_account_id,
        }

    def to_synthesize_speech_params(self) -> dict:
        return self.model_dump(
            exclude_none=True,
            exclude={
                "aws_access_key_id",
                "aws_secret_access_key",
                "aws_session_token",
                "region_name",
                "profile_name",
                "aws_account_id",
            },
            by_alias=True,
        )


class PollyTTS:
    def __init__(
        self,
        params: PollyTTSParams,
        thread_pool_size: int = 1,
        timeout: float = 5.0,
        max_retries: int = 3,
        retry_delay: float = 0.1,
        chunk_interval_ms: int = 50,
    ) -> None:
        self.params = params
        # calculate frame size with chunk interval
        self.frame_size = int(
            int(params.sample_rate) * 1 * 2 * chunk_interval_ms / 1000
        )

        self.thread_pool = ThreadPoolExecutor(max_workers=thread_pool_size)
        self._closed = False
        self.timeout = timeout
        self.max_retries = max_retries
        self.retry_delay = retry_delay

        self.session = boto3.Session(**params.to_session_params())
        self.client = self.session.client(
            "polly", config=Config(tcp_keepalive=True)
        )

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        if not self._closed:
            self.thread_pool.shutdown(wait=True)
            self._closed = True

    def __del__(self):
        self.close()

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

    def synthesize_speech(self, text: str) -> Iterator[bytes]:
        response = self.client.synthesize_speech(
            Text=text, **self.params.to_synthesize_speech_params()
        )

        if "AudioStream" not in response:
            raise ValueError("No audio stream in response")

        with closing(response["AudioStream"]) as stream:
            for chunk in stream.iter_chunks(chunk_size=self.frame_size):
                yield chunk

    async def async_synthesize_speech(
        self, text: str, timeout: float = 30.0
    ) -> AsyncIterator[bytes]:
        """
        async synthesize speech and return the audio stream

        Args:
            text: the text to synthesize
            timeout: the timeout in seconds

        Yields:
            bytes: the audio data chunk

        Raises:
            ValueError: when the response does not contain an audio stream
            ClientError: when the AWS Polly API call fails
            asyncio.TimeoutError: when the operation times out
        """
        # if not text.strip():
        #     logging.warning("empty text input")
        #     return

        try:
            # run the sync synthesize_speech method in the thread pool with timeout
            loop = asyncio.get_event_loop()

            # use asyncio.to_thread instead of run_in_executor (Python 3.9+)
            if hasattr(asyncio, "to_thread"):
                sync_iterator = await asyncio.wait_for(
                    asyncio.to_thread(self.synthesize_speech, text),
                    timeout=timeout,
                )
            else:
                sync_iterator = await asyncio.wait_for(
                    loop.run_in_executor(
                        self.thread_pool, self.synthesize_speech, text
                    ),
                    timeout=timeout,
                )

            # use the helper method to convert the sync iterator to an async iterator
            async for chunk in self._async_iter_from_sync(sync_iterator):
                yield chunk

        except asyncio.TimeoutError:
            logging.error(f"speech synthesis timeout ({timeout} seconds)")
            raise
        except asyncio.CancelledError:
            logging.info("speech synthesis task is cancelled")
            raise
        except ClientError as e:
            error_code = e.response["Error"]["Code"]
            error_message = e.response["Error"]["Message"]
            logging.error(
                f"AWS Polly API error [{error_code}]: {error_message}"
            )
            raise
        except ValueError as e:
            logging.error(f"audio stream error: {e}")
            raise
        except Exception as e:
            logging.error(f"unknown error in speech synthesis: {e}")
            logging.error(traceback.format_exc())
            raise

    async def async_synthesize_speech_with_retry(
        self, text: str
    ) -> AsyncIterator[bytes]:
        """
        async synthesize speech with retry mechanism

        Args:
            text: the text to synthesize

        Yields:
            bytes: the audio data chunk
        """
        for attempt in range(self.max_retries + 1):
            try:
                async for chunk in self.async_synthesize_speech(
                    text, timeout=self.timeout
                ):
                    yield chunk
                return  # success, exit the retry loop

            except (asyncio.TimeoutError, ClientError) as e:
                if attempt < self.max_retries:
                    logging.warning(
                        f"speech synthesis failed (attempt {attempt + 1}/{self.max_retries + 1}): {e}"
                    )
                    await asyncio.sleep(
                        self.retry_delay * (attempt + 1)
                    )  # exponential backoff
                else:
                    logging.error(
                        f"speech synthesis finally failed, retried {self.max_retries} times: {e}"
                    )
                    raise
            except Exception as e:
                logging.error(f"unexpected error in speech synthesis: {e}")
                raise


if __name__ == "__main__":
    import os
    import time

    aws_access_key_id = os.getenv("AWS_TTS_ACCESS_KEY_ID", "")
    aws_secret_access_key = os.getenv("AWS_TTS_SECRET_ACCESS_KEY", "")
    region_name = os.getenv("AWS_TTS_REGION", "")
    # configure logging
    logging.basicConfig(level=logging.INFO)

    params = PollyTTSParams(
        aws_access_key_id=aws_access_key_id,
        aws_secret_access_key=aws_secret_access_key,
        region_name=region_name,
    )
    print("configuration parameters:", params.model_dump_json())

    # use context manager to ensure resources are cleaned up correctly
    with PollyTTS(params) as polly:

        async def main():
            # pre connect to aws polly
            async for chunk in polly.async_synthesize_speech("P"):
                ...

            t = time.time()
            print("test speech synthesis:")
            async for chunk in polly.async_synthesize_speech_with_retry(
                "Hello world!"
            ):
                print(
                    f"received audio chunk delay: {time.time() - t} seconds, chunk: {len(chunk)} bytes"
                )
            print("speech synthesis done")

            await asyncio.sleep(30)

            t = time.time()
            print("\nsecond test speech synthesis:")
            async for chunk in polly.async_synthesize_speech_with_retry(
                "Hello world!"
            ):
                print(
                    f"received audio chunk delay: {time.time() - t} seconds, chunk: {len(chunk)} bytes"
                )
            print("second speech synthesis done")

        asyncio.run(main())
