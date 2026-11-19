#
# Breeze TTS extension for the TEN Framework.
# Licensed under the Apache License, Version 2.0.
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import BreezeTTSExtension


@register_addon_as_extension("breeze_tts_python")
class BreezeTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("BreezeTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(BreezeTTSExtension(name), context)
