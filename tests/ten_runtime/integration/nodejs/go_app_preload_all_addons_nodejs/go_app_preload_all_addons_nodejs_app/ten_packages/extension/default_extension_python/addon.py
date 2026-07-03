#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import time
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
    LogLevel,
)
from .extension import DefaultExtension

# Sleep 3 seconds to mock the long import time of the addon.
time.sleep(3)
print("default_extension_python addon loaded")


@register_addon_as_extension("default_extension_python")
class DefaultExtensionAddon(Addon):
    def on_create_instance(
        self, ten_env: TenEnv, name: str, context: object
    ) -> None:
        ten_env.log(LogLevel.INFO, "on_create_instance")
        ten_env.on_create_instance_done(DefaultExtension(name), context)
