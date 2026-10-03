#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("speechmatics_tts_python")
class SpeechmaticsTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import SpeechmaticsTTSExtension

        ten_env.log_info("SpeechmaticsTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(SpeechmaticsTTSExtension(name), context)
