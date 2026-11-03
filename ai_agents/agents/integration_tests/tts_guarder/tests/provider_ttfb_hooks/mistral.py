"""Observe mistral HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.mistral_tts_python.mistral_tts"
    client_class = "MistralTTSClient"
