"""
Configuration for the Gradium real-time speech-to-speech translation
(MLLM) extension.

Field names and the region/base_url resolution logic intentionally mirror
gradium_asr_python/config.py and gradium_tts_python/config.py -- those two
extensions are already wired up against Gradium's real ASR and TTS
websocket APIs (wss://<region>.api.gradium.ai/api/speech/{asr,tts}), so this
reuses everything confirmed there (auth header, region hosts, message
shapes).

The one field that is NOT confirmed against real docs is `path`: Gradium's
combined speech-to-speech Translation API endpoint hasn't been documented to
us yet. "/api/speech/s2s" below is a best guess extrapolated from the
asr/tts pattern (and the "s2s-websocket" label referenced on
gradium.ai/translate). Update it, and this docstring, once Gradium confirms
the real path.
"""

from typing import Any, Literal

from pydantic import BaseModel


class GradiumMLLMConfig(BaseModel):
    api_key: str = ""
    """API key for Gradium API authentication (sent as the x-api-key header)."""

    region: Literal["eu", "us"] = "us"
    """Gradium API region. Options: 'eu' (Europe) or 'us' (USA)."""

    base_url: str = ""
    """Optional explicit override of the websocket host (skips region-based lookup)."""

    path: str = "/api/speech/s2s"
    """Websocket path for the speech-to-speech endpoint. UNCONFIRMED -- see module docstring."""

    model_name: str = "default"
    """Name of the Gradium speech-to-speech model to use."""

    stt_model_name: str = ""
    """Optional override for the ASR leg of the pipeline, if Gradium exposes one."""

    tts_model_name: str = ""
    """Optional override for the TTS leg of the pipeline, if Gradium exposes one."""

    voice_id: str = ""
    """Voice used for the synthesized (translated) speech output."""

    language: str = ""
    """Source language hint (if supported); leave empty for auto-detect."""

    target_language: str = "pt"
    """Language to translate into."""

    input_format: str = "pcm"
    """Audio input format, matching gradium_asr_python's convention."""

    output_format: str = "pcm"
    """Audio output format, matching gradium_tts_python's convention."""

    input_sample_rate: int = 24000
    """Audio sample rate (Hz) Gradium expects for input PCM."""

    dump: bool = False
    """Enable audio dumping for debugging."""

    dump_path: str = "/tmp"
    """Path to dump audio files when debugging."""

    def websocket_url(self) -> str:
        """Build the websocket URL based on region, or an explicit base_url override."""
        if self.base_url:
            return f"{self.base_url}{self.path}"
        region = self.region if self.region in ("us", "eu") else "us"
        return f"wss://{region}.api.gradium.ai{self.path}"

    def output_sample_rate(self) -> int:
        """Return sample rate based on output_format, matching gradium_tts_python's convention."""
        fmt = self.output_format.lower()
        if fmt == "pcm_16000":
            return 16000
        if fmt == "pcm_24000":
            return 24000
        return 48000

    def setup_message(self) -> dict[str, Any]:
        """
        Combined ASR+TTS 'setup' payload for the speech-to-speech session.

        Merges gradium_asr_python's setup fields (model_name, input_format,
        language) with gradium_tts_python's (model_name, voice_id,
        output_format), plus translation-specific fields. Optional fields
        are omitted when unset, matching gradium_asr_python's handling of
        `language`.
        """
        message: dict[str, Any] = {
            "type": "setup",
            "model_name": self.model_name,
            "input_format": self.input_format,
            "output_format": self.output_format,
        }
        if self.stt_model_name:
            message["stt_model_name"] = self.stt_model_name
        if self.tts_model_name:
            message["tts_model_name"] = self.tts_model_name
        if self.voice_id:
            message["voice_id"] = self.voice_id
        if self.language:
            message["language"] = self.language
        if self.target_language:
            message["target_language"] = self.target_language
        return message
