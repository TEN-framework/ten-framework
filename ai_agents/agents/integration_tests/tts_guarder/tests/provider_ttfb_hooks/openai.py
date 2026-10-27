"""Independent openai HTTP provider TTFB observer."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.openai_tts2_python.openai_tts"
    audio_keys = {"audio"}
    binding = "AsyncClient"
