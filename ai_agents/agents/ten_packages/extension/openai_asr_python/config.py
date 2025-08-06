from pydantic import BaseModel, Field
from pathlib import Path
from .utils import encrypting_serializer
from .openai_asr_client import TranscriptionParam


class OpenAIASRConfig(BaseModel):
    api_key: str = Field(..., description="OpenAI API key")
    organization: str | None = Field(default=None, description="OpenAI organization")
    project: str | None = Field(default=None, description="OpenAI project")
    websocket_base_url: str | None = Field(default=None, description="OpenAI websocket base url")
    params: TranscriptionParam = Field(..., description="OpenAI ASR params")
    dump: bool = Field(default=False, description="OpenAI ASR dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "openai_asr_in.pcm"),
        description="OpenAI ASR dump path",
    )
    log_level: str = Field(
        default="INFO", description="OpenAI ASR log level"
    )

    _encrypt_serializer = encrypting_serializer(
        "api_key",
        "organization",
        "project"
    )
