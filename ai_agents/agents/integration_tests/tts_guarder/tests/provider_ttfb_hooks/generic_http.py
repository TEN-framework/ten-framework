"""Observe generic_http HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.generic_tts_http_python.generic_tts"
    client_class = "GenericTTSClient"
