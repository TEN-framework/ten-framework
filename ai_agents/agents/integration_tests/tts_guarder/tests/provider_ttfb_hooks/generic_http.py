"""Generic HTTP raw audio response and request-body transport boundaries."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.generic_tts_http_python.generic_tts"
    # This adapter's contract is raw PCM, not an arbitrary JSON TTS envelope.
    audio_keys = set()
