#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("nvidia_riva_tts_python")
class NvidiaRivaTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import NvidiaRivaTTSExtension

        ten_env.log_info("NvidiaRivaTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(NvidiaRivaTTSExtension(name), context)
