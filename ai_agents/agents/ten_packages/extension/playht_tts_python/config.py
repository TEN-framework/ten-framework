from pydantic import BaseModel, Field
from pathlib import Path
from .playht_tts import PlayHTParams


class PlayHTTTSConfig(BaseModel):
    """PlayHT TTS Config"""

    dump: bool = Field(default=False, description="PlayHT TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "playht_tts_in.pcm"
        ),
        description="PlayHT TTS dump path",
    )
    params: PlayHTParams = Field(..., description="PlayHT TTS params")
