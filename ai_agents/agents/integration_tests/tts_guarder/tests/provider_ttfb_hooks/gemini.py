"""Gemini inlineData audio in JSON/SSE provider responses."""

from .http import HttpxHook


class Hook(HttpxHook):
    module_name = "ten_packages.extension.gemini_tts_python.gemini_tts"
    audio_keys = {"inlineData", "inline_data"}
    binding = "httpx"
