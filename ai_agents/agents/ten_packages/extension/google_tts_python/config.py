from typing import Any, Dict, List
from pydantic import BaseModel, Field
from google.cloud import texttospeech


def mask_sensitive_data(
    s: str, unmasked_start: int = 3, unmasked_end: int = 3, mask_char: str = "*"
) -> str:
    if not s or len(s) <= unmasked_start + unmasked_end:
        return mask_char * len(s)

    return (
        s[:unmasked_start]
        + mask_char * (len(s) - unmasked_start - unmasked_end)
        + s[-unmasked_end:]
    )


class GoogleTTSConfig(BaseModel):
    credentials: str = ""
    language_code: str = "en-US"
    voice_name: str = ""
    ssml_gender: str = "NEUTRAL"
    speaking_rate: float = 1.0
    pitch: float = 0.0
    volume_gain_db: float = 0.0
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    sample_rate: int = 24000
    black_list_keys: List[str] = ["credentials"]

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.credentials:
            config.credentials = mask_sensitive_data(config.credentials)
        return f"{config}"

    def update_params(self) -> None:
        # This function allows overriding default config values with 'params' from property.json

        # self.params is a Dict[str, Any] as defined in the model
        params_dict: Dict[str, Any] = self.params
        # pylint: disable=no-member

        for key, value in params_dict.items():
            if hasattr(self, key):
                setattr(self, key, value)

        # Delete keys after iteration is complete
        for key in self.black_list_keys:
            if key in params_dict:
                del params_dict[key]

    def get_ssml_gender(self) -> texttospeech.SsmlVoiceGender:
        """Convert string gender to Google TTS enum"""
        gender_map = {
            "NEUTRAL": texttospeech.SsmlVoiceGender.NEUTRAL,
            "MALE": texttospeech.SsmlVoiceGender.MALE,
            "FEMALE": texttospeech.SsmlVoiceGender.FEMALE,
            "UNSPECIFIED": texttospeech.SsmlVoiceGender.SSML_VOICE_GENDER_UNSPECIFIED,
        }
        return gender_map.get(
            self.ssml_gender.upper(), texttospeech.SsmlVoiceGender.NEUTRAL
        )
