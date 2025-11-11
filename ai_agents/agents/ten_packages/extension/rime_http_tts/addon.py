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


@register_addon_as_extension("rime_http_tts")
class RimeTTSExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import RimeTTSExtension

        ten_env.log_info("RimeTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(RimeTTSExtension(name), context)
