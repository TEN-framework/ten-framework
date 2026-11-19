"""Observe gmicloud HTTP adapter request and audio boundaries."""

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.gmicloud_tts2_python.client"
    client_class = "GmicloudTTSClient"
