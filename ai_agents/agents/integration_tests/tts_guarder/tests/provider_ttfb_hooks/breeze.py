"""Observe breeze HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.breeze_tts_python.breeze_tts"
    client_class = "BreezeTTSClient"
