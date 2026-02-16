#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

from ten_runtime import Addon, register_addon_as_extension, TenEnv


@register_addon_as_extension("whisper_stt_python")
class WhisperSTTExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import WhisperSTTExtension

        ten_env.log_info("WhisperSTTExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(WhisperSTTExtension(name), context)
