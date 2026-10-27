"""Independent hume HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.humeai_tts_python.humeTTS"
    audio_keys = {"audio"}
    binding = "httpx"
