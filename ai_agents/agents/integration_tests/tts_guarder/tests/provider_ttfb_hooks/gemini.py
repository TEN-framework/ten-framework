"""Observe gemini HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.gemini_tts_python.gemini_tts"
    client_class = "GeminiTTSClient"
