"""
Blaze STT Extension Implementation

This extension wraps the Blaze STT API endpoint for use in TEN framework.
"""

import os
import logging
from typing import Optional, Dict, Any, BinaryIO, Union
from io import BytesIO

import httpx
from pydantic import BaseModel, Field

# Import UploadFile for multipart support
try:
    from fastapi import UploadFile
except ImportError:
    # Fallback if fastapi is not available
    UploadFile = None

logger = logging.getLogger(__name__)


class BlazeSTTConfig(BaseModel):
    """Configuration for Blaze STT Extension"""
    
    api_url: str = Field(
        default=os.getenv("BLAZE_STT_API_URL", "http://localhost:8000"),
        description="Blaze STT API base URL"
    )
    api_key: Optional[str] = Field(
        default=os.getenv("BLAZE_STT_API_KEY", None),
        description="API key for authentication (Bearer token)"
    )
    timeout: int = Field(
        default=3600,
        description="Request timeout in seconds"
    )
    enable_segments: bool = Field(
        default=False,
        description="Split audio into segments with timestamps"
    )
    enable_refinement: bool = Field(
        default=False,
        description="Apply post-processing refinement to improve accuracy"
    )
    default_language: str = Field(
        default="vi",
        description="Default language code (e.g., 'vi' for Vietnamese)"
    )


class BlazeSTTExtension:
    """
    Blaze STT Extension for TEN Framework
    
    This extension provides Speech-to-Text functionality by wrapping
    the Blaze STT API endpoint: /v1/stt/execute
    
    Implements TEN framework extension interface with process() and get_metadata() methods.
    """
    
    def __init__(self, config: Optional[Union[BlazeSTTConfig, Dict[str, Any]]] = None):
        """
        Initialize Blaze STT Extension
        
        Args:
            config: Configuration object (BlazeSTTConfig) or dict from TEN framework.
                   If None, uses environment variables.
                   If dict, converts to BlazeSTTConfig.
        """
        if config is None:
            self.config = BlazeSTTConfig()
        elif isinstance(config, dict):
            # Convert dict from TEN framework to BlazeSTTConfig
            self.config = BlazeSTTConfig(
                api_url=config.get("api_url", "http://localhost:8000"),
                api_key=config.get("api_key"),
                default_language=config.get("language", "vi"),
                enable_segments=config.get("enable_segments", False),
                enable_refinement=config.get("enable_refinement", False),
                timeout=config.get("timeout", 3600),
            )
        else:
            self.config = config
        
        self.base_url = self.config.api_url.rstrip("/")
        self.endpoint = f"{self.base_url}/v1/stt/execute"
        
        logger.info(f"Blaze STT Extension initialized with API URL: {self.base_url}")
    
    def transcribe(
        self,
        audio_data: Optional[bytes] = None,
        audio_file: Optional[UploadFile] = None,
        audio_content_type: Optional[str] = None,
        language: Optional[str] = None,
        enable_segments: Optional[bool] = None,
        enable_refinement: Optional[bool] = None,
        lazy_process: bool = False,
    ) -> Dict[str, Any]:
        """
        Transcribe audio data to text
        
        Similar to API endpoint /v1/stt/execute which accepts:
        - UploadFile via multipart/form-data (field name: audio_file)
        - Binary data in request body with Content-Type header
        
        Args:
            audio_data: Binary audio data (bytes). Required if audio_file is None.
            audio_file: FastAPI UploadFile object (sent as multipart/form-data).
                       If provided, audio_data is ignored.
            audio_content_type: MIME type. Auto-detected if not provided.
            language: Language code (e.g., 'vi' for Vietnamese). Defaults to config default.
            enable_segments: Split audio into segments with timestamps
            enable_refinement: Apply post-processing refinement
            lazy_process: If True, process in background (returns job_id). If False, returns result immediately.
        
        Returns:
            Dict containing transcription result or job information
            
        Raises:
            httpx.HTTPError: If the API request fails
            ValueError: If both audio_data and audio_file are None, or if audio_data is empty
        """
        if audio_file is None and audio_data is None:
            raise ValueError("Either audio_data or audio_file must be provided")
        
        if audio_file is not None and audio_data is not None:
            logger.warning("Both audio_file and audio_data provided. audio_file will be used.")
        
        # Use provided values or fall back to config defaults
        language = language or self.config.default_language
        enable_segments = enable_segments if enable_segments is not None else self.config.enable_segments
        enable_refinement = enable_refinement if enable_refinement is not None else self.config.enable_refinement
        
        # Prepare headers
        headers = {}
        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"
        
        # Prepare query parameters
        params = {
            "language": language,
            "enable_segments": str(enable_segments).lower(),
            "enable_refinement": str(enable_refinement).lower(),
            "lazy_process": str(lazy_process).lower(),
        }
        
        try:
            with httpx.Client(timeout=self.config.timeout) as client:
                if audio_file is not None:
                    if UploadFile is None:
                        raise ImportError("fastapi is required to use audio_file parameter. Install with: pip install fastapi")
                    
                    # Reset file pointer if needed
                    if hasattr(audio_file.file, 'seek'):
                        audio_file.file.seek(0)
                    
                    # Get filename and content type
                    filename = getattr(audio_file, 'filename', 'audio.mp3') or 'audio.mp3'
                    content_type = audio_content_type or getattr(audio_file, 'content_type', None) or "audio/mpeg"
                    
                    # Infer content type from filename if needed
                    if content_type == "application/octet-stream" or not content_type:
                        ext = os.path.splitext(filename)[1].lower()
                        if ext == ".wav":
                            content_type = "audio/wav"
                        elif ext in [".mp3", ".mpeg"]:
                            content_type = "audio/mpeg"
                    
                    files = {
                        "audio_file": (filename, audio_file.file, content_type)
                    }
                    
                    response = client.post(
                        self.endpoint,
                        files=files,
                        headers=headers,
                        params=params,
                    )
                
                else:
                    if not audio_data:
                        raise ValueError("audio_data cannot be empty")
                    
                    content_type = audio_content_type or "audio/wav"
                    headers["Content-Type"] = content_type
                    
                    response = client.post(
                        self.endpoint,
                        content=audio_data,
                        headers=headers,
                        params=params,
                    )
                
                response.raise_for_status()
                result = response.json()
                
                # Handle response format from service
                # Response structure:
                # - lazy_process=False: {"job_status": "completed", "result": {"data": {"transcription": "..."}}}
                # - lazy_process=True: {"job_id": "...", "job_status": "processing"}
                
                # Extract transcription from nested result.data structure if available
                transcription = ""
                if result.get("result") and isinstance(result["result"], dict):
                    result_data = result["result"].get("data", {})
                    if isinstance(result_data, dict):
                        transcription = result_data.get("transcription", "")
                
                # Return normalized format
                return {
                    "transcription": transcription,
                    "job_id": result.get("job_id"),
                    "job_status": result.get("job_status", "processing"),
                    "raw_result": result,  # Include full result for advanced use cases
                }
        
        except httpx.HTTPStatusError as e:
            logger.error(f"Blaze STT API error: {e.response.status_code} - {e.response.text}")
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze STT request error: {str(e)}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error in Blaze STT: {str(e)}")
            raise
    
    def get_job_status(self, job_id: str) -> Dict[str, Any]:
        """
        Get status of a transcription job
        
        Args:
            job_id: Job ID returned from transcribe with lazy_process=True
        
        Returns:
            Dict containing job status and result if available
            Format: {
                "job_id": "...",
                "job_status": "processing" | "completed" | "failed",
                "transcription": "...",  # Extracted from result.data.transcription
                "result": {...}  # Full result structure
            }
        """
        headers = {}
        if self.config.api_key:
            headers["Authorization"] = f"Bearer {self.config.api_key}"
        
        endpoint = f"{self.base_url}/v1/stt/{job_id}"
        
        try:
            with httpx.Client(timeout=30) as client:
                response = client.get(endpoint, headers=headers)
                response.raise_for_status()
                result = response.json()
                
                # Extract transcription from nested result.data structure if available
                transcription = ""
                if result.get("result") and isinstance(result["result"], dict):
                    result_data = result["result"].get("data", {})
                    if isinstance(result_data, dict):
                        transcription = result_data.get("transcription", "")
                
                # Return normalized format
                return {
                    "job_id": result.get("job_id", job_id),
                    "job_status": result.get("job_status", "processing"),
                    "transcription": transcription,
                    "result": result.get("result"),
                    "raw_result": result,  # Include full result for advanced use cases
                }
        
        except httpx.HTTPStatusError as e:
            logger.error(f"Blaze STT job status error: {e.response.status_code} - {e.response.text}")
            raise
        except httpx.RequestError as e:
            logger.error(f"Blaze STT request error: {str(e)}")
            raise
    
    def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process input according to TEN framework interface
        
        This method implements the TEN framework extension interface.
        
        Args:
            input_data: Input dict with:
                - audio_data (bytes): Required. Audio data to transcribe
                - audio_content_type (str): Optional. MIME type (default: "audio/wav")
                - language (str): Optional. Language code (default: from config)
                - enable_segments (bool): Optional. Enable segments
                - enable_refinement (bool): Optional. Enable refinement
                - lazy_process (bool): Optional. Process in background (default: False)
        
        Returns:
            Output dict with:
                - transcription (str): Transcribed text
                - job_id (str): Optional. Job ID if lazy_process=True
                - status (str): Job status
        """
        audio_data = input_data.get("audio_data")
        if not audio_data:
            raise ValueError("audio_data is required in input_data")
        
        result = self.transcribe(
            audio_data=audio_data,
            audio_content_type=input_data.get("audio_content_type", "audio/wav"),
            language=input_data.get("language"),
            enable_segments=input_data.get("enable_segments"),
            enable_refinement=input_data.get("enable_refinement"),
            lazy_process=input_data.get("lazy_process", False),
        )
        
        # Return normalized format (transcribe() already handles response format)
        return {
            "transcription": result.get("transcription", ""),
            "job_id": result.get("job_id"),
            "status": result.get("job_status", "completed"),
            "raw_result": result,  # Include full result for advanced use cases
        }
    
    def get_metadata(self) -> Dict[str, Any]:
        """
        Return extension metadata for TEN framework
        
        This method implements the TEN framework extension interface.
        
        Returns:
            Dict with extension information
        """
        return {
            "name": "blaze_stt_python",
            "version": "1.0.0",
            "description": "Blaze Speech-to-Text extension for TEN framework",
            "capabilities": ["stt", "transcription", "speech_to_text"],
            "supported_formats": ["audio/wav", "audio/mpeg", "audio/webm", "audio/ogg"],
            "supported_languages": ["vi", "en"],
            "config_schema": {
                "api_url": {"type": "string", "required": False, "default": "http://localhost:8000"},
                "api_key": {"type": "string", "required": False},
                "language": {"type": "string", "required": False, "default": "vi"},
                "enable_segments": {"type": "boolean", "required": False, "default": False},
                "enable_refinement": {"type": "boolean", "required": False, "default": False},
            },
        }

