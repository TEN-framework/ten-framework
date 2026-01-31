#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import AsyncIterator
import numpy as np
import riva.client
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

from .config import NvidiaRivaTTSConfig


class NvidiaRivaTTSClient:
    """NVIDIA Riva TTS Client implementation"""

    def __init__(
        self,
        config: NvidiaRivaTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.auth = None
        self.tts_service = None

        try:
            # Initialize Riva client
            server = config.params["server"]
            use_ssl = config.params.get("use_ssl", False)

            self.ten_env.log_info(
                f"Initializing NVIDIA Riva TTS client with server: {server}, SSL: {use_ssl}",
                category=LOG_CATEGORY_VENDOR,
            )

            self.auth = riva.client.Auth(use_ssl=use_ssl, uri=server)
            self.tts_service = riva.client.SpeechSynthesisService(self.auth)

            self.ten_env.log_info(
                "NVIDIA Riva TTS client initialized successfully",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            ten_env.log_error(
                f"Error when initializing NVIDIA Riva TTS: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"Error when initializing NVIDIA Riva TTS: {e}"
            ) from e

    async def cancel(self):
        """Cancel the current TTS request"""
        self.ten_env.log_debug("NVIDIA Riva TTS: cancel() called.")
        self._is_cancelled = True

    async def synthesize(
        self, text: str, request_id: str
    ) -> AsyncIterator[bytes]:
        """
        Synthesize speech from text using NVIDIA Riva TTS.

        Args:
            text: Text to synthesize
            request_id: Unique request identifier

        Yields:
            Audio data as bytes (PCM format)
        """
        self._is_cancelled = False

        if not self.tts_service:
            self.ten_env.log_error(
                f"NVIDIA Riva TTS: service not initialized for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"NVIDIA Riva TTS: service not initialized for request_id: {request_id}"
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"NVIDIA Riva TTS: empty text for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            return

        try:
            language_code = self.config.params["language_code"]
            voice_name = self.config.params["voice_name"]
            sample_rate = self.config.params.get("sample_rate", 16000)

            self.ten_env.log_debug(
                f"NVIDIA Riva TTS: synthesizing text (length: {len(text)}) "
                f"with voice: {voice_name}, language: {language_code}, "
                f"sample_rate: {sample_rate}, request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Use streaming synthesis for lower latency
            responses = self.tts_service.synthesize_online(
                text,
                voice_name=voice_name,
                language_code=language_code,
                sample_rate_hz=sample_rate,
                encoding=riva.client.AudioEncoding.LINEAR_PCM,
            )

            # Stream audio chunks
            for response in responses:
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation detected, stopping TTS stream for request_id: {request_id}"
                    )
                    break

                # Convert audio bytes to numpy array and back to bytes
                # This ensures proper format
                audio_data = np.frombuffer(response.audio, dtype=np.int16)

                self.ten_env.log_debug(
                    f"NVIDIA Riva TTS: yielding audio chunk, "
                    f"length: {len(audio_data)} samples, request_id: {request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

                yield audio_data.tobytes()

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"NVIDIA Riva TTS: synthesis completed for request_id: {request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"NVIDIA Riva TTS: error during synthesis: {error_message}, "
                f"request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"NVIDIA Riva TTS synthesis failed: {error_message}"
            ) from e
