from pydantic import BaseModel, Field
from pathlib import Path
from .azure_tts import AzureTTSParams


class AzureTTSConfig(BaseModel):
    """Azure TTS Config"""

    dump: bool = Field(default=False, description="Azure TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "azure_tts_in.pcm"),
        description="Azure TTS dump path",
    )
    pre_connect: bool = Field(default=True, description="Azure TTS pre connect")
    chunk_size: int = Field(
        default=3200, description="Azure TTS chunk size in bytes"
    )
    params: AzureTTSParams = Field(..., description="Azure TTS params")
