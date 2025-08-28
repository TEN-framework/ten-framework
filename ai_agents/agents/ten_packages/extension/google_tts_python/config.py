from typing import Any, Dict, List
from pydantic import BaseModel, Field
from google.cloud import texttospeech
from ten_ai_base import utils


class GoogleTTSConfig(BaseModel):
    credentials: str = ""
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_keys: List[str] = ["credentials"]

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.credentials:
            config.credentials = utils.encrypt(config.credentials)
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
        # pylint: disable=no-member
        voice_params = self.params.get("VoiceSelectionParams", {})
        ssml_gender = voice_params.get("ssml_gender", "NEUTRAL")
        return gender_map.get(
            ssml_gender.upper(), texttospeech.SsmlVoiceGender.NEUTRAL
        )
