"""
Blaze TTS Extension Implementation

This extension wraps the Blaze TTS API endpoint for use in TEN framework.
"""

import os
import logging
from typing import Optional, Dict, Any, Union
from enum import Enum

import httpx
from pydantic import BaseModel, Field

logger = logging.getLogger(__name__)


class AudioFormat(str, Enum):
    """Audio format options"""

    WAV = "wav"
    MP3 = "mp3"
    OGG = "ogg"


class MediaType(str, Enum):
    """Media type options"""

    AUDIO_OGG_CODECS_OPUS = "audio/ogg; codecs=opus"
    AUDIO_MP3 = "audio/mp3"
    AUDIO_WAV = "audio/wav"


class Normalization(str, Enum):
    """Normalization options"""

    NO = "no"
    YES = "yes"


class Model(str, Enum):
    """Model options"""

    V1_5_FLASH = "v1.5_flash"
    V1_5_PRO = "v1.5_pro"


class BlazeTTSConfig(BaseModel):
    """Configuration for Blaze TTS Extension"""

    api_url: str = Field(
        default=os.getenv("BLAZE_TTS_API_URL", "http://localhost:8000"),
        description="Blaze TTS API base URL",
    )
    api_key: Optional[str] = Field(
        default=os.getenv("BLAZE_TTS_API_KEY", None),
        description="API key for authentication (Bearer token)",
    )
    timeout: int = Field(default=3600, description="Request timeout in seconds")
    default_language: str = Field(
        default="vi",
        description="Default language code (e.g., 'vi' for Vietnamese)",
    )
    default_speaker_id: Optional[str] = Field(
        default=None, description="Default speaker ID"
    )
    default_audio_speed: float = Field(
        default=1.0, description="Default audio speed multiplier"
    )
    default_audio_quality: int = Field(
        default=64, description="Default audio quality (kbps)"
    )


class BlazeTTSExtension:
    """
    Blaze TTS Extension for TEN Framework

    This extension provides Text-to-Speech functionality by wrapping
    the Blaze TTS API endpoint: /v1/tts

    Implements TEN framework extension interface with process() and get_metadata() methods.
    """

    def __init__(
        self, config: Optional[Union[BlazeTTSConfig, Dict[str, Any]]] = None
    ):
        """
        Initialize Blaze TTS Extension

        Args:
            config: Configuration object (BlazeTTSConfig) or dict from TEN framework.
                   If None, uses environment variables.
                   If dict, converts to BlazeTTSConfig.
        """
        if config is None:
            self.config = BlazeTTSConfig()
        elif isinstance(config, dict):
            # Convert dict from TEN framework to BlazeTTSConfig
            self.config = BlazeTTSConfig(
                api_url=config.get("api_url", "http://localhost:8000"),
                api_key=config.get("api_key"),
                default_language=config.get("language", "vi"),
                default_speaker_id=config.get("speaker_id"),
                default_audio_speed=config.get("audio_speed", 1.0),
                default_audio_quality=config.get("audio_quality", 64),
                timeout=config.get("timeout", 3600),
            )
        else:
            self.config = config

        self.base_url = self.config.api_url.rstrip("/")
        self.endpoint = f"{self.base_url}/v1/tts"

        logger.info(
            f"Blaze TTS Extension initialized with API URL: {self.base_url}"
        )

    def synthesize(
        self,
        text: str,
        speaker_id: Optional[str] = None,
        language: Optional[str] = None,
        audio_speed: Optional[float] = None,
        audio_quality: Optional[int] = None,
        audio_format: Union[AudioFormat, str] = AudioFormat.WAV,
        media_type: Union[MediaType, str] = MediaType.AUDIO_OGG_CODECS_OPUS,
        normalization: Union[Normalization, str] = Normalization.NO,
        model: Union[Model, str] = Model.V1_5_PRO,
    ) -> Dict[str, Any]:
        """
        Synthesize text to speech

        Args:
            text: Text to synthesize
            speaker_id: Speaker/voice ID. Required if not set in config.
            language: Language code (e.g., 'vi' for Vietnamese). Defaults to config default.
            audio_speed: Audio speed multiplier (default: 1.0)
            audio_quality: Audio quality in kbps (default: 64)
            audio_format: Audio format (wav, mp3, ogg)
            media_type: Media type
            normalization: Normalization option (no, yes)
            model: Model version to use

        Returns:
            Dict containing TTS result with job_id or audio URL

        Raises:
            httpx.HTTPError: If the API request fails
            ValueError: If text is empty or speaker_id is missing
        """
        if not text:
            raise ValueError("text cannot be empty")

        speaker_id = speaker_id or self.config.default_speaker_id
        if not speaker_id:
            raise ValueError(
                "speaker_id is required (either as parameter or in config)"
            )

        # Use provided values or fall back to config defaults
        language = language or self.config.default_language
        audio_speed = (
            audio_speed
            if audio_speed is not None
            else self.config.default_audio_speed
        )
        audio_quality = (
            audio_quality
            if audio_quality is not None
            else self.config.default_audio_quality
        )

        # Convert enum to string if needed
        if isinstance(audio_format, AudioFormat):
            audio_format = audio_format.value
        if isinstance(media_type, MediaType):
            media_type = media_type.value
        if isinstance(normalization, Normalization):
            normalization = normalization.value
        if isinstance(model, Model):
            model = model.value

        # Prepare request payload
        payload = {
            "query": text,
            "language": language,
            "audio_speed": audio_speed,
            "audio_quality": audio_quality,
            "audio_format": audio_format,
            "speaker_id": speaker_id,
            "media_type": media_type,
            "normalization": normalization,
            "model": model,
        }

        # Prepare headers
        headers = {
            "Content-Type": "application/json",
        }

        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"

        try:
            with httpx.Client(timeout=self.config.timeout) as client:
                response = client.post(
                    self.endpoint,
                    json=payload,
                    headers=headers,
                )
                response.raise_for_status()
                return response.json()

        except httpx.HTTPStatusError as e:
            logger.error(
                f"Blaze TTS API error: {e.response.status_code} - {e.response.text}"
            )
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze TTS request error: {str(e)}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error in Blaze TTS: {str(e)}")
            raise

    def get_speakers(self) -> Dict[str, Any]:
        """
        Get list of available speakers/voices

        Returns:
            Dict containing list of speakers
        """
        headers = {}
        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"

        endpoint = f"{self.base_url}/v1/tts/list-speaker-ids"

        try:
            with httpx.Client(timeout=30) as client:
                response = client.get(endpoint, headers=headers)
                response.raise_for_status()
                return response.json()

        except httpx.HTTPStatusError as e:
            logger.error(
                f"Blaze TTS speakers error: {e.response.status_code} - {e.response.text}"
            )
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze TTS request error: {str(e)}")
            raise

    def download_audio(
        self, job_id: str, output_path: Optional[str] = None
    ) -> bytes:
        """
        Download generated audio file

        Args:
            job_id: Job ID returned from synthesize
            output_path: Optional path to save the audio file. If None, returns bytes.

        Returns:
            Audio file bytes
        """
        headers = {}
        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"

        endpoint = f"{self.base_url}/v1/tts/{job_id}/download"

        try:
            with httpx.Client(timeout=self.config.timeout) as client:
                response = client.get(endpoint, headers=headers)
                response.raise_for_status()

                audio_bytes = response.content

                if output_path:
                    with open(output_path, "wb") as f:
                        f.write(audio_bytes)
                    logger.info(f"Audio saved to {output_path}")

                return audio_bytes

        except httpx.HTTPStatusError as e:
            logger.error(
                f"Blaze TTS download error: {e.response.status_code} - {e.response.text}"
            )
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze TTS request error: {str(e)}")
            raise

    def get_job_info(self, job_id: str) -> Dict[str, Any]:
        """
        Get information about a TTS job

        Args:
            job_id: Job ID returned from synthesize

        Returns:
            Dict containing job information
        """
        headers = {}
        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"

        endpoint = f"{self.base_url}/v1/tts/{job_id}/info"

        try:
            with httpx.Client(timeout=30) as client:
                response = client.get(endpoint, headers=headers)
                response.raise_for_status()
                return response.json()

        except httpx.HTTPStatusError as e:
            logger.error(
                f"Blaze TTS job info error: {e.response.status_code} - {e.response.text}"
            )
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze TTS request error: {str(e)}")
            raise

    def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process input according to TEN framework interface

        This method implements the TEN framework extension interface.

        Args:
            input_data: Input dict with:
                - text (str): Required. Text to synthesize
                - speaker_id (str): Optional. Speaker ID (default: from config)
                - language (str): Optional. Language code (default: from config)
                - audio_speed (float): Optional. Audio speed (default: 1.0)
                - audio_quality (int): Optional. Audio quality in kbps (default: 64)
                - audio_format (str): Optional. Audio format (default: "wav")
                - download_audio (bool): Optional. Download audio immediately (default: True)

        Returns:
            Output dict with:
                - audio_data (bytes): Audio bytes if download_audio=True
                - job_id (str): Job ID
                - format (str): Audio format
                - status (str): Job status
        """
        text = input_data.get("text")
        if not text:
            raise ValueError("text is required in input_data")

        result = self.synthesize(
            text=text,
            speaker_id=input_data.get("speaker_id"),
            language=input_data.get("language"),
            audio_speed=input_data.get("audio_speed", 1.0),
            audio_quality=input_data.get("audio_quality", 64),
            audio_format=input_data.get("audio_format", "wav"),
            media_type=input_data.get(
                "media_type", MediaType.AUDIO_OGG_CODECS_OPUS
            ),
            normalization=input_data.get("normalization", "no"),
            model=input_data.get("model", Model.V1_5_PRO),
        )

        job_id = result.get("id") or result.get("job_id")

        # If immediate result requested, download audio
        if job_id and input_data.get("download_audio", True):
            try:
                audio_bytes = self.download_audio(job_id)
                return {
                    "audio_data": audio_bytes,
                    "job_id": job_id,
                    "format": input_data.get("audio_format", "mp3"),
                    "status": "completed",
                    "size_bytes": len(audio_bytes),
                }
            except Exception as e:
                # If download fails, return job_id for later retrieval
                return {
                    "job_id": job_id,
                    "status": "processing",
                    "error": str(e),
                }

        return {
            "job_id": job_id,
            "status": "processing",
        }

    def get_metadata(self) -> Dict[str, Any]:
        """
        Return extension metadata for TEN framework

        This method implements the TEN framework extension interface.

        Returns:
            Dict with extension information
        """
        return {
            "name": "blaze_tts_python",
            "version": "1.0.0",
            "description": "Blaze Text-to-Speech extension for TEN framework",
            "capabilities": ["tts", "synthesis", "text_to_speech"],
            "supported_formats": ["mp3", "wav", "ogg"],
            "supported_languages": ["vi", "en"],
            "config_schema": {
                "api_url": {
                    "type": "string",
                    "required": False,
                    "default": "http://localhost:8000",
                },
                "api_key": {"type": "string", "required": False},
                "language": {
                    "type": "string",
                    "required": False,
                    "default": "vi",
                },
                "speaker_id": {"type": "string", "required": False},
                "audio_speed": {
                    "type": "float",
                    "required": False,
                    "default": 1.0,
                },
                "audio_quality": {
                    "type": "integer",
                    "required": False,
                    "default": 64,
                },
            },
        }
