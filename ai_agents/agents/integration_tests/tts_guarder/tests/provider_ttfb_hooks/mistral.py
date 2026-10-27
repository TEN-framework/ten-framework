"""Independent mistral HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.mistral_tts_python.mistral_tts"
    audio_keys = {"audio_data"}
    binding = "AsyncClient"
