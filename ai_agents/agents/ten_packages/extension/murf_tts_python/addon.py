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


@register_addon_as_extension("murf_tts_python")
class MurfTTSExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import MurfTTSExtension

        ten_env.log_info("MurfTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(MurfTTSExtension(name), context)
