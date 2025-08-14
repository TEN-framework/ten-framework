from pydantic import BaseModel, Field
from pathlib import Path
from .polly_tts import PollyTTSParams


class PollyTTSConfig(BaseModel):
    """Amazon Polly TTS Config"""

    dump: bool = Field(default=False, description="Amazon Polly TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "polly_tts_in.pcm"),
        description="Amazon Polly TTS dump path",
    )
    params: PollyTTSParams = Field(..., description="Amazon Polly TTS params")
