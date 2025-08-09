from pydantic import BaseModel, Field, ConfigDict
from pathlib import Path
from typing import Optional, Dict, Any, Literal
from .utils import encrypting_serializer
from amazon_transcribe.auth import StaticCredentialResolver


class AWSTranscriptionConfig(BaseModel):
    """AWS Transcription Config"""

    region: str = Field(..., description="AWS region, e.g. 'us-west-2'")
    access_key_id: str = Field(..., description="AWS access key id")
    secret_access_key: str = Field(..., description="AWS secret access key")

    language_code: str = Field(..., description="Language code, e.g. 'en-US', 'zh-CN'")
    media_sample_rate_hz: int = Field(

        ..., description="Audio sample rate (Hz), e.g. 16000"
    )
    media_encoding: str = Field(..., description="Audio encoding format, e.g. 'pcm'")
    vocabulary_name: Optional[str] = Field(
        default=None, description="Custom vocabulary name"
    )
    session_id: Optional[str] = Field(default=None, description="Session ID")
    vocab_filter_method: Optional[str] = Field(
        default=None, description="Vocabulary filter method"
    )
    vocab_filter_name: Optional[str] = Field(
        default=None, description="Vocabulary filter name"
    )
    show_speaker_label: Optional[bool] = Field(
        default=None, description="Whether to show speaker label"
    )
    enable_channel_identification: Optional[bool] = Field(
        default=None, description="Whether to enable channel identification"
    )
    number_of_channels: Optional[int] = Field(
        default=None, description="Number of channels"
    )
    enable_partial_results_stabilization: Optional[bool] = Field(
        default=None, description="Whether to enable partial results stabilization"
    )
    partial_results_stability: Optional[str] = Field(
        default=None, description="Partial results stability setting"
    )
    language_model_name: Optional[str] = Field(
        default=None, description="Language model name"
    )

    model_config = ConfigDict(extra="allow")

    def to_transcription_params(self) -> Dict[str, Any]:
        """
        Convert config to start_stream_transcription parameters

        Returns:
            Dict[str, Any]: Parameters that can be directly passed to start_stream_transcription
        """
        return self.model_dump(
            exclude_none=True, exclude={"region", "access_key_id", "secret_access_key"}
        )

    def to_client_params(self) -> Dict[str, Any]:
        """
        Convert config to client parameters
        """
        return {
            "region": self.region,
            "credential_resolver": StaticCredentialResolver(
                access_key_id=self.access_key_id,
                secret_access_key=self.secret_access_key,
            )
        }

    _encrypt_serializer = encrypting_serializer("access_key_id", "secret_access_key")


class AWSASRConfig(BaseModel):
    """AWS ASR Config"""

    dump: bool = Field(default=False, description="AWS ASR dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "aws_asr_in.pcm"),
        description="AWS ASR dump path",
    )
    log_level: str = Field(default="INFO", description="AWS ASR log level")
    finalize_mode: Literal["disconnect", "mute_pkg"] = Field(default="disconnect", description="AWS ASR finalize mode")
    mute_pkg_duration_ms: int = Field(default=800, description="AWS ASR mute pkg duration (ms)")
    params: AWSTranscriptionConfig = Field(..., description="AWS ASR params")
    
