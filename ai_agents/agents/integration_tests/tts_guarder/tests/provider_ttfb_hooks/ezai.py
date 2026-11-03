"""Observe ezai HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.ezai_tw_tts_python.ezai_tts"
    client_class = "EZAITWTTSClient"
