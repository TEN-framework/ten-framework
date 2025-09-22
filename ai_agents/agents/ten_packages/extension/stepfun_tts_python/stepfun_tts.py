import asyncio
from typing import AsyncIterator
from openai import OpenAI
from ten_runtime import AsyncTenEnv
from .config import StepFunTTSConfig
import time
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_REQUEST_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5


class StepFunTTS:
    def __init__(
        self,
        config: StepFunTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env = ten_env
        self.client = None
        self._initialize_client()
        self.send_text_in_connection = False
        self.cur_request_id = ""

    def _initialize_client(self):
        """Initialize StepFun TTS client with API key"""
        try:
            if not self.config.api_key:
                raise ValueError("API key is required for StepFun TTS")

            # Create OpenAI client with StepFun base URL
            self.client = OpenAI(
                api_key=self.config.api_key,
                base_url=self.config.base_url
            )
            self.ten_env.log_debug("StepFun TTS client initialized successfully")

        except Exception as e:
            self.ten_env.log_error(
                f"Failed to initialize StepFun TTS client: {e}"
            )
            raise

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """Generate TTS audio for the given text"""

        if not self.client:
            error_msg = "StepFun TTS client not initialized"
            self.ten_env.log_error(error_msg)
            yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, None
            return

        # Retry configuration
        max_retries = 3
        retry_delay = 1.0  # seconds

        # Retry loop for network issues
        for attempt in range(max_retries):
            ttfb_ms = None
            try:
                start_ts = None
                if request_id != self.cur_request_id:
                    start_ts = time.time()
                    self.cur_request_id = request_id

                # Prepare request parameters
                request_params = {
                    "model": self.config.get_model(),
                    "voice": self.config.get_voice(),
                    "input": text,
                    "response_format": self.config.get_response_format(),
                    "speed": self.config.get_speed(),
                    "volume": self.config.get_volume(),
                    "sample_rate": self.config.get_sample_rate(),
                }

                # Add voice_label if specified
                voice_label = self.config.get_voice_label()
                if voice_label:
                    request_params["extra_body"] = {
                        "voice_label": voice_label
                    }

                # Perform the text-to-speech request
                response = self.client.audio.speech.create(**request_params)
                
                if start_ts is not None:
                    ttfb_ms = int((time.time() - start_ts) * 1000)
                self.send_text_in_connection = True

                # Get audio content
                audio_content = response.content
                if audio_content:
                    yield audio_content, EVENT_TTS_RESPONSE, ttfb_ms
                    yield None, EVENT_TTS_REQUEST_END, ttfb_ms
                    return  # Success, exit retry loop
                else:
                    error_msg = "No audio content received from StepFun TTS"
                    yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
                    return

            except Exception as e:
                error_message = str(e)
                self.ten_env.log_error(
                    f"vendor_error: reason: {error_message}",
                    category=LOG_CATEGORY_VENDOR,
                )

                # Check if it's a retryable network error
                is_retryable = (
                    ("503" in error_message and "UNAVAILABLE" in error_message)
                    or ("failed to connect" in error_message.lower())
                    or ("socket closed" in error_message.lower())
                    or ("timeout" in error_message.lower())
                    or ("connection" in error_message.lower())
                )

                if is_retryable and attempt < max_retries - 1:
                    self.ten_env.log_debug(
                        f"Network error (attempt {attempt + 1}/{max_retries}): {error_message}"
                    )
                    self.ten_env.log_debug(
                        f"Retrying in {retry_delay} seconds..."
                    )
                    await asyncio.sleep(retry_delay)
                    retry_delay *= 2  # Exponential backoff
                    continue
                else:
                    # Final attempt failed or non-retryable error
                    self.ten_env.log_error(f"StepFun TTS synthesis failed: {e}")

                    # Check if it's an authentication error
                    if (
                        (
                            "401" in error_message
                            and "Unauthorized" in error_message
                        )
                        or (
                            "403" in error_message
                            and "Forbidden" in error_message
                        )
                        or ("authentication" in error_message.lower())
                        or ("api_key" in error_message.lower())
                        or ("invalid" in error_message.lower() and "key" in error_message.lower())
                    ):
                        yield error_message.encode(
                            "utf-8"
                        ), EVENT_TTS_INVALID_KEY_ERROR, ttfb_ms
                    # Check if it's a network error
                    elif (
                        (
                            "503" in error_message
                            and "UNAVAILABLE" in error_message
                        )
                        or ("failed to connect" in error_message.lower())
                        or ("socket closed" in error_message.lower())
                        or ("network" in error_message.lower())
                    ):
                        network_error = f"Network connection failed after {max_retries} attempts: {error_message}. Please check your internet connection and StepFun service availability."
                        yield network_error.encode(
                            "utf-8"
                        ), EVENT_TTS_ERROR, ttfb_ms
                    else:
                        yield error_message.encode(
                            "utf-8"
                        ), EVENT_TTS_ERROR, ttfb_ms
                    return

    def clean(self):
        """Clean up resources"""
        self.ten_env.log_info("StepFunTTS: clean() called.")
        if self.client:
            self.client = None
            self.ten_env.log_debug("StepFun TTS client cleaned")

    async def reset(self):
        """Reset the client"""
        self.ten_env.log_info("Resetting StepFun TTS client")
        self.client = None
        self._initialize_client()
        self.ten_env.log_debug("StepFun TTS client reset completed")




