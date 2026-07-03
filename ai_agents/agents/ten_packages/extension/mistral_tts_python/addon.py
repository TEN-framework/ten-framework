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

from .extension import MistralTTSExtension


@register_addon_as_extension("mistral_tts_python")
class MistralTTSExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:

        ten_env.log_info("MistralTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(MistralTTSExtension(name), context)
