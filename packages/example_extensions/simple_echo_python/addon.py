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
from .extension import SimpleEchoExtension


@register_addon_as_extension("simple_echo_python")
class SimpleEchoExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.on_create_instance_done(SimpleEchoExtension(name), context)
