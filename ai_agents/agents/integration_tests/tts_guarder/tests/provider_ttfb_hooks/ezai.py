"""Independent ezai HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.ezai_tw_tts_python.ezai_tts"
    audio_keys = {"audio"}
    binding = "AsyncClient"
