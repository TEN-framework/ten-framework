import asyncio
import os
import time
from queue import Queue, Empty
from typing import Callable, Awaitable, Any

from google.cloud import speech_v2 as speech
from google.cloud.speech_v2.types import (
    StreamingRecognitionConfig,
    StreamingRecognizeRequest,
)
from google.api_core import exceptions as gcp_exceptions
import grpc

from ten_ai_base.struct import ASRResult, ASRWord
from ten_runtime import AsyncTenEnv

from .config import GoogleASRConfig


class GoogleASRClient:
    """Google Cloud Speech-to-Text V2 streaming client"""

    def __init__(
        self,
        config: GoogleASRConfig,
        ten_env: AsyncTenEnv,
        on_result_callback: Callable[[ASRResult], Awaitable[None]],
        on_error_callback: Callable[[int, str], Awaitable[None]],
    ):
        self.config = config
        self.ten_env = ten_env
        self.on_result_callback = on_result_callback
        self.on_error_callback = on_error_callback

        self.speech_client: speech.SpeechAsyncClient | None = None
        self._audio_queue: Queue = Queue()
        self._recognition_task: asyncio.Task | None = None
        self._stop_event = asyncio.Event()
        self.is_finalizing = False

    async def start(self) -> None:
        """Initializes the client and starts the recognition stream."""
        self.ten_env.log_info("Starting Google ASR client...")
        try:
            await self._initialize_google_client()
            self._stop_event.clear()
            self.is_finalizing = False
            self._recognition_task = asyncio.create_task(
                self._run_recognition()
            )
            self.ten_env.log_info("Google ASR client started successfully.")
        except Exception as e:
            self.ten_env.log_error(f"Failed to start Google ASR client: {e}")
            await self.on_error_callback(
                500, f"Failed to start client: {str(e)}"
            )
            raise

    async def _initialize_google_client(self) -> None:
        """Initializes the Google Speech async client using ADC."""
        try:
            # Check for ADC credentials path from config or environment
            credentials_path = self.config.adc_credentials_path or os.getenv('GOOGLE_APPLICATION_CREDENTIALS')

            self.ten_env.log_info(f"ADC credentials path: {credentials_path}")

            if credentials_path:
                self.ten_env.log_info(f"Using Service Account credentials from: {credentials_path}")
                if not os.path.exists(credentials_path):
                    raise FileNotFoundError(f"Service Account file not found: {credentials_path}")

                # Set the environment variable for Google Cloud SDK
                os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = credentials_path
            else:
                self.ten_env.log_info("No ADC credentials path specified, using default ADC")

            # Create client with ADC (Application Default Credentials)
            # This will automatically use the best available credentials
            if credentials_path:
                # Explicitly pass credentials to ensure they are used
                from google.auth import default
                credentials, project = default()
                self.speech_client = speech.SpeechAsyncClient(credentials=credentials)
                self.ten_env.log_info(
                    f"Initialized Google Speech V2 client with explicit credentials from {credentials_path}"
                )
            else:
                # Use default ADC
                self.speech_client = speech.SpeechAsyncClient()
                self.ten_env.log_info(
                    "Initialized Google Speech V2 client with Application Default Credentials"
                )
        except Exception as e:
            self.ten_env.log_error(
                f"Failed to initialize Google Speech client: {e}"
            )
            raise

    async def stop(self) -> None:
        """Stops the recognition stream and cleans up resources."""
        self.ten_env.log_info("Stopping Google ASR client...")
        if self._recognition_task and not self._recognition_task.done():
            self._stop_event.set()
            # Drain the queue to unblock the generator
            while not self._audio_queue.empty():
                self._audio_queue.get_nowait()
            self._audio_queue.put(None)  # Signal generator to stop
            try:
                await asyncio.wait_for(self._recognition_task, timeout=5.0)
            except asyncio.TimeoutError:
                self.ten_env.log_warn(
                    "Recognition task did not stop gracefully, cancelling."
                )
                self._recognition_task.cancel()
        self.speech_client = None
        self.ten_env.log_info("Google ASR client stopped.")

    async def send_audio(self, chunk: bytes) -> None:
        """Adds an audio chunk to the processing queue."""
        if not self._stop_event.is_set():
            self._audio_queue.put(chunk)

    async def finalize(self) -> None:
        """Signals that the current utterance is complete."""
        self.ten_env.log_info("Finalizing utterance.")
        self.is_finalizing = True
        self._audio_queue.put(None)  # Signal end of audio stream

    async def _audio_generator(self):
        """Yields audio chunks from the queue for the gRPC stream."""
        try:
            # First request contains the configuration
            config = self.config.get_recognition_config()
            streaming_config = StreamingRecognitionConfig(
                config=config,
                streaming_features={
                    "interim_results": self.config.interim_results
                },
            )
            recognizer_path = self.config.get_recognizer_path()

            self.ten_env.log_info(f"Using recognizer: {recognizer_path}")
            self.ten_env.log_info(f"Using streaming config: {streaming_config}")

            # First request must contain recognizer and streaming_config, not audio
            # recognizer format: projects/{project}/locations/{location}/recognizers/{recognizer}
            # According to Google Cloud Speech V2 docs, recognizer is required
            recognizer_path = self.config.get_recognizer_path()
            if recognizer_path:
                self.ten_env.log_info(f"Using recognizer: {recognizer_path}")
                yield StreamingRecognizeRequest(
                    recognizer=recognizer_path,
                    streaming_config=streaming_config
                )
            else:
                # If no recognizer path, we need to get project_id from ADC
                # This is a fallback for when project_id is not provided in config
                self.ten_env.log_error("No recognizer path available. Please provide project_id in config.")
                raise ValueError("Recognizer path is required for Google Cloud Speech V2 API")

            while not self._stop_event.is_set():
                try:
                    chunk = self._audio_queue.get_nowait()
                    if chunk is None:
                        self.ten_env.log_info("Received end-of-stream signal")
                        break
                    # self.ten_env.log_info(
                    #     f"Sending audio chunk of size {len(chunk)} bytes"
                    # )
                    yield speech.StreamingRecognizeRequest(audio=chunk)
                except Empty:
                    await asyncio.sleep(0.01)  # Wait for more audio
        except Exception as e:
            self.ten_env.log_error(f"Error in audio generator: {e}")
            await self.on_error_callback(500, str(e))

    async def _run_recognition(self) -> None:
        """Run the streaming recognition loop with retry logic."""
        retry_count = 0
        while (
            not self._stop_event.is_set()
            and retry_count < self.config.max_retry_attempts
        ):
            try:
                if not self.speech_client:
                    raise ConnectionError(
                        "Google Speech client is not initialized."
                    )
                requests = self._audio_generator()
                self.ten_env.log_info("Starting streaming recognition...")
                try:
                    responses = await self.speech_client.streaming_recognize(
                        requests=requests
                    )
                    self.ten_env.log_info("Got streaming response iterator")
                    self.ten_env.log_info(f"Responses: {responses}")

                    async for response in responses:
                        if self._stop_event.is_set():
                            break
                        self.ten_env.log_info(f"Received response: {response}")
                        await self._process_response(response)
                except Exception as e:
                    self.ten_env.log_error(f"Error in streaming_recognize: {e}")
                    await self.on_error_callback(500, str(e))

                # If the loop finishes without exceptions, reset retry count
                retry_count = 0
                if self.is_finalizing:
                    break  # Clean exit after finalize

            except (gcp_exceptions.GoogleAPICallError, grpc.RpcError) as e:
                error_code = (
                    e.code().value[0]
                    if hasattr(e, "code") and hasattr(e.code(), "value")
                    else 500
                )
                error_message = e.details() if hasattr(e, "details") else str(e)

                # Provide more specific error information for common issues
                if "IAM_PERMISSION_DENIED" in error_message:
                    self.ten_env.log_error(
                        f"Google Cloud Speech API permission denied ({error_code}): {error_message}"
                    )
                    self.ten_env.log_error(
                        "Please ensure the service account has the following roles:"
                    )
                    self.ten_env.log_error(
                        "- Speech-to-Text API User (roles/speech.client)"
                    )
                    self.ten_env.log_error(
                        "- Or custom role with 'speech.recognizers.recognize' permission"
                    )
                else:
                    self.ten_env.log_error(
                        f"Google API/gRPC error ({error_code}): {error_message}"
                    )

                await self.on_error_callback(error_code, error_message)

                if self._is_retryable_error(e):
                    retry_count += 1
                    self.ten_env.log_warn(
                        f"Retryable error encountered. Attempt {retry_count}/{self.config.max_retry_attempts}. Retrying in {self.config.retry_delay}s..."
                    )
                    await asyncio.sleep(self.config.retry_delay)
                else:
                    self.ten_env.log_error(
                        "Non-retryable error encountered. Stopping recognition."
                    )
                    break  # Non-retryable error

            except Exception as e:
                self.ten_env.log_error(
                    f"Unexpected error in recognition loop: {e}"
                )
                await self.on_error_callback(500, str(e))
                break

    async def _process_response(
        self, response: speech.StreamingRecognizeResponse
    ) -> None:
        """Process a streaming recognition response and trigger callbacks."""
        self.ten_env.log_info(
            f"Processing response with {len(response.results)} results"
        )
        for result in response.results:
            if not result.alternatives:
                self.ten_env.log_info("Skipping result with no alternatives")
                continue

            # We'll use the first alternative as the primary result.
            first_alt = result.alternatives[0]
            words = [
                ASRWord(
                    text=word.word,
                    start_ms=int(word.start_offset.total_seconds() * 1000),
                    duration_ms=int(
                        (word.end_offset - word.start_offset).total_seconds()
                        * 1000
                    ),
                    confidence=word.confidence,
                )
                for word in first_alt.words
            ]

            asr_result = ASRResult(
                is_final=result.is_final,
                text=first_alt.transcript,
                words=words,
                confidence=first_alt.confidence,
                language=result.language_code,
                start_ms=(int(words[0].start_ms) if words else 0),
                duration_ms=(
                    int(
                        words[-1].start_ms
                        + words[-1].duration_ms
                        - words[0].start_ms
                    )
                    if words
                    else 0
                ),
            )
            await self.on_result_callback(asr_result)

    def _is_retryable_error(self, error: Exception) -> bool:
        """Check if a gRPC/API error is retryable."""
        if isinstance(error, gcp_exceptions.RetryError):
            return True
        if isinstance(error, grpc.RpcError):
            # List of retryable gRPC status codes
            retryable_codes = [
                grpc.StatusCode.UNAVAILABLE,
                grpc.StatusCode.DEADLINE_EXCEEDED,
                grpc.StatusCode.RESOURCE_EXHAUSTED,
                grpc.StatusCode.INTERNAL,
            ]
            # Don't retry permission denied errors
            if error.code() == grpc.StatusCode.PERMISSION_DENIED:
                return False
            return error.code() in retryable_codes
        return False
