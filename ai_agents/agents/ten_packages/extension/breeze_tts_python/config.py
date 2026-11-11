from pathlib import Path

from pydantic import BaseModel, Field

try:
    from .breeze_tts import BreezeTTSParams
except ImportError:  # running outside the TEN package
    from breeze_tts import BreezeTTSParams


class BreezeTTSConfig(BaseModel):
    """Extension configuration; ``params`` mirrors Breeze's request fields."""

    dump: bool = Field(default=False, description="Dump synthesized PCM to disk")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "breeze_tts_in.pcm"),
        description="Dump file or directory",
    )
    params: BreezeTTSParams = Field(..., description="Breeze TTS parameters")
