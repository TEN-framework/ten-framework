"""Observe openai HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.openai_tts2_python.openai_tts"
    client_class = "OpenAITTSClient"
