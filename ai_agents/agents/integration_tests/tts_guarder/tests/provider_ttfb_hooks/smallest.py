"""Independent smallest HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.smallest_tts_python.smallest_tts"
    audio_keys = {"audio"}
    binding = "AsyncClient"
