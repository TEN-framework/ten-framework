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


@register_addon_as_extension("elevenlabs_tts2_python")
class ElevenLabsTTS2ExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import ElevenLabsTTS2Extension

        ten_env.log_info("ElevenLabsTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(ElevenLabsTTS2Extension(name), context)
