from typing import Literal

from pydantic import BaseModel, ConfigDict, SecretStr
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class TimbreParams(BaseModel):
    model_config = ConfigDict(hide_input_in_errors=True)
    api_key: SecretStr = SecretStr("")
    model: Literal["timbre-v2.5", "timbre-v3.0"] = "timbre-v3.0"
    voice: str = "Shivani"
    language: Literal[
        "auto",
        "en-IN",
        "hi-IN",
        "ta-IN",
        "te-IN",
        "kn-IN",
        "ml-IN",
        "mr-IN",
        "pa-IN",
        "bn-IN",
        "gu-IN",
        "hi-en",
    ] = "en-IN"
    sample_rate: Literal[8000, 16000, 22050, 24000, 44100, 48000] = 24000


class GnaniTTSConfig(AsyncTTS2HttpConfig):
    model_config = ConfigDict(hide_input_in_errors=True)
    params: TimbreParams = TimbreParams()

    def update_params(self):
        self.params.voice = self.params.voice.strip()

    def validate(self):
        if not self.params.api_key.get_secret_value().strip():
            raise ValueError("Gnani API key is required")
        if not self.params.voice:
            raise ValueError("Timbre voice is required")

    def to_str(self, sensitive_handling=True):
        return self.model_dump_json(exclude={"params": {"api_key"}})
