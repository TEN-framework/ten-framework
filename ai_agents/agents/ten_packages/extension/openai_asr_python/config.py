from pydantic import BaseModel, Field, ConfigDict
from pathlib import Path
from .utils import encrypting_serializer
from .openai_asr_client import TranscriptionParam


class Params(BaseModel):
    api_key: str = Field(..., description="OpenAI API key")
    organization: str | None = Field(
        default=None, description="OpenAI organization"
    )
    project: str | None = Field(default=None, description="OpenAI project")
    base_url: str | None = Field(
        default=None, description="OpenAI websocket base url"
    )
    log_level: str = Field(default="INFO", description="OpenAI ASR log level")
    sample_rate: int = Field(
        default=16000, description="Set the extension input audio sample rate"
    )

    model_config = ConfigDict(extra="allow")

    _encrypt_serializer = encrypting_serializer(
        "api_key", "organization", "project"
    )

    def to_transcription_param(self) -> TranscriptionParam:
        return TranscriptionParam(
            **self.model_dump(
                exclude_none=True,
                exclude={
                    "api_key",
                    "organization",
                    "project",
                    "base_url",
                    "log_level",
                    "sample_rate",
                },
            ),
        )


class OpenAIASRConfig(BaseModel):
    dump: bool = Field(default=False, description="OpenAI ASR dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "openai_asr_in.pcm"
        ),
        description="OpenAI ASR dump path",
    )
    params: Params = Field(..., description="OpenAI ASR params")
