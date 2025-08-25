from pydantic import BaseModel, Field
from pathlib import Path
from .groq_tts import GroqTTSParams


class GroqTTSConfig(BaseModel):
    """Groq TTS Config"""

    dump: bool = Field(default=False, description="Groq TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "groq_tts_in.pcm"),
        description="Groq TTS dump path",
    )
    params: GroqTTSParams = Field(..., description="Groq TTS params")
