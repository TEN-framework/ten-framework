#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("vibevoice_tts_websocket_python")
class VibeVoiceTTSWebsocketExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import VibeVoiceTTSWebsocketExtension

        ten_env.log_info(
            "VibeVoiceTTSWebsocketExtensionAddon on_create_instance"
        )
        ten_env.on_create_instance_done(
            VibeVoiceTTSWebsocketExtension(name), context
        )
