#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("gmicloud_tts2_python")
class GmicloudTTS2ExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import GmicloudTTS2Extension

        ten_env.log_info("GmicloudTTS2ExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(GmicloudTTS2Extension(name), context)
