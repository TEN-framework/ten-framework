"""Observe hume HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.humeai_tts_python.humeTTS"
    client_class = "HumeAiTTS"
