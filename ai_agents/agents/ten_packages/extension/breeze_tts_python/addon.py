#
# Breeze TTS extension for the TEN Framework.
# Licensed under the Apache License, Version 2.0.
#
from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("breeze_tts_python")
class BreezeTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import BreezeTTSExtension

        ten_env.log_info("breeze tts on_create_instance")
        ten_env.on_create_instance_done(BreezeTTSExtension(name), context)
