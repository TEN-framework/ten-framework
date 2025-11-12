#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any
from pydantic import BaseModel, Field


class GoogleASRConfig(BaseModel):
    """Google Cloud Speech-to-Text V2 ASR configuration

    Refer to: https://cloud.google.com/speech-to-text/v2/docs/libraries
    """

    # Google Cloud credentials and project settings
    project_id: str = (
        ""  # Google Cloud Project ID (optional, will use ADC if not provided)
    )
    location: str = "global"  # Google Cloud location
    adc_credentials_path: str = ""  # Path to ADC credentials file (optional)
    adc_credentials_string: str = ""  # ADC credentials string (optional)

    def get_client_options(self) -> dict[str, str]:
        """Get client options for Google Cloud Speech client."""
        return {
            "project_id": self.project_id,
        }

    # Audio configuration
    sample_rate: int = 16000  # Audio sample rate in Hz
    channels: int = 1  # Number of audio channels
    encoding: str = "LINEAR16"  # Audio encoding (LINEAR16, FLAC, MULAW, etc.)

    # Language and model settings
    language: str = "en-US"  # Primary language code
    language_list: list[str] = Field(
        default_factory=list
    )  # Alternative language codes
    model: str = "long"  # Recognition model (e.g., "long", "short", "chirp_2").

    # Recognition settings
    enable_automatic_punctuation: bool = True
    enable_word_time_offsets: bool = True
    enable_speaker_diarization: bool = False
    diarization_speaker_count: int = 0  # Number of speakers (0 = auto detect)
    max_alternatives: int = 1  # Maximum number of recognition alternatives

    # Streaming settings
    single_utterance: bool = (
        False  # Not used in V2 streaming_features; kept for compatibility
    )
    interim_results: bool = True  # Enable interim results

    # Content filtering
    profanity_filter: bool = False  # Enable profanity filter

    # Speech contexts for better recognition (phrases and boost values)
    speech_contexts: list[dict[str, Any]] = Field(default_factory=list)

    # Audio processing
    use_enhanced: bool = False  # Use enhanced model (premium pricing)

    # Adaptation settings
    adaptation_phrase_set_references: list[str] = Field(default_factory=list)
    adaptation_custom_class_references: list[str] = Field(default_factory=list)

    # Timeout settings
    recognition_timeout: int = 60  # Recognition timeout in seconds
    connection_timeout: int = 30  # Connection timeout in seconds
    stream_max_duration: int = (
        270  # Max stream duration in seconds (Google default is 300s, we use 270s to be safe)
    )

    # Retry settings
    max_retry_attempts: int = 3  # Maximum retry attempts on failure
    retry_delay: float = 1.0  # Delay between retry attempts in seconds

    # Extension configuration
    params: dict[str, Any] = Field(default_factory=dict)
    black_list_params: list[str] = Field(default_factory=list)

    # Audio dumping settings
    dump: bool = False
    dump_path: str = "."

    # Logging
    enable_detailed_logging: bool = False
    finalize_grace_seconds: float = 0.5

    def is_black_list_params(self, key: str) -> bool:
        """Check if a parameter key is in the blacklist."""
        return key in list(self.black_list_params)

    def update(self, params: dict[str, Any]) -> None:
        """Update configuration with provided parameters."""
        for key, value in params.items():
            if hasattr(self, key) and not self.is_black_list_params(key):
                setattr(self, key, value)

        # Process language_list from comma-separated language string
        if "," in self.language:
            self.language_list = [
                lang.strip() for lang in self.language.split(",")
            ]
        elif self.language and not self.language_list:
            self.language_list = [self.language]

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string."""
        if not sensitive_handling:
            return self.model_dump_json()

        # Handle sensitive data for logging/debugging
        config = self.model_copy(deep=True)
        if config.project_id:
            config.project_id = "***"

        return config.model_dump_json()

    def get_recognition_config(self) -> dict[str, Any]:
        """Get Google Cloud Speech V2 recognition config dictionary."""
        # Prefer explicit decoding config for raw PCM or known uncontainerized audio
        # This avoids "unsupported encoding" errors when auto-decoding can't infer format
        config: dict[str, Any] = {
            "language_codes": (
                self.language_list if self.language_list else [self.language]
            ),
            "model": self.model,
            "features": {
                "enable_automatic_punctuation": self.enable_automatic_punctuation,
                "enable_word_time_offsets": self.enable_word_time_offsets,
                "profanity_filter": self.profanity_filter,
                "max_alternatives": self.max_alternatives,
            },
        }

        encoding_value = (self.encoding or "").strip().lower()

        # Map common encodings to Speech V2 explicit decoding
        # If user explicitly sets "auto", fallback to auto_decoding_config
        if encoding_value and encoding_value != "auto":
            # Normalize common names
            encoding_map = {
                "linear16": "LINEAR16",
                "pcm16": "LINEAR16",
                "pcm_s16le": "LINEAR16",
                "mulaw": "MULAW",
                "alaw": "ALAW",
                "flac": "FLAC",
            }
            mapped = encoding_map.get(encoding_value, encoding_value.upper())
            config["explicit_decoding_config"] = {
                "encoding": mapped,
                "sample_rate_hertz": int(self.sample_rate),
                "audio_channel_count": int(self.channels),
            }
        else:
            # Let server auto-detect containerized/compressed formats like wav/mp3/ogg
            config["auto_decoding_config"] = {}

        # Add speaker diarization config if enabled (V2 structure)
        if self.enable_speaker_diarization:
            diarization_config = {}
            if self.diarization_speaker_count > 0:
                diarization_config["min_speaker_count"] = (
                    self.diarization_speaker_count
                )
                diarization_config["max_speaker_count"] = (
                    self.diarization_speaker_count
                )
            config["features"]["diarization_config"] = diarization_config

        # Add speech contexts if provided (V2 structure)
        if self.speech_contexts:
            config["features"]["speech_contexts"] = self.speech_contexts

        # Add adaptation settings if provided (V2 structure)
        adaptation_config = {}
        if self.adaptation_phrase_set_references:
            adaptation_config["phrase_set_references"] = (
                self.adaptation_phrase_set_references
            )
        if self.adaptation_custom_class_references:
            adaptation_config["custom_class_references"] = (
                self.adaptation_custom_class_references
            )
        if adaptation_config:
            config["adaptation"] = adaptation_config

        return config

    def get_recognizer_path(self) -> str:
        """Get the recognizer path for V2 API."""
        # According to Google Cloud Speech V2 docs, recognizer is required
        # Use default recognizer "_" to avoid permission issues
        if self.project_id and self.location != "global":
            return f"projects/{self.project_id}/locations/{self.location}/recognizers/_"
        elif self.project_id:
            return f"projects/{self.project_id}/locations/global/recognizers/_"
        else:
            # If no project_id, we'll need to get it from ADC
            return ""

    def validate_config(self) -> tuple[bool, str]:
        """Validate the configuration and return (is_valid, error_message)."""
        errors: list[str] = []

        # For ADC authentication, project_id is optional (will be retrieved from ADC)
        # No validation errors for missing project_id as it will be retrieved from ADC

        if errors:
            return False, "; ".join(errors)
        return True, ""
