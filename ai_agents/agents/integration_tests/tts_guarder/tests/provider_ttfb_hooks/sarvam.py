"""Independent sarvam HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.sarvam_http_tts.sarvam_tts"
    audio_keys = {"audios"}
    binding = "AsyncClient"
