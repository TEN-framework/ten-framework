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
    timeout: float = Field(default=30.0, description="Amazon Polly TTS timeout")
    max_retries: int = Field(
        default=3, description="Amazon Polly TTS max retries"
    )
    retry_delay: float = Field(
        default=1.0, description="Amazon Polly TTS retry delay in seconds"
    )
    chunk_interval_ms: int = Field(
        default=50,
        description="Amazon Polly TTS chunk interval in milliseconds",
    )
