"""Observe the installed Timbre HTTP adapter, including TEN loader imports."""

import sys

from .http import HttpAdapterHook


class Hook(HttpAdapterHook):
    module_name = "ten_packages.extension.gnani_tts_python.gnani_tts"
    client_class = "GnaniTTSClient"

    def install(self, patch, reference, config):
        # After a graph has run, the TEN loader may have imported the addon
        # under the app's package prefix. Observe that same client class.
        for name in tuple(sys.modules):
            if name.endswith("gnani_tts_python.gnani_tts"):
                self.module_name = name
                break
        super().install(patch, reference, config)
