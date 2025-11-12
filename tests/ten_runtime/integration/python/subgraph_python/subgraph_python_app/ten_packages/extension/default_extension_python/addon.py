#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
    LogLevel,
)
from .extension import (
    PowerExtension,
    MultiExtension,
    SubstractExtension,
    FunctionEntryExtension,
)


@register_addon_as_extension("default_extension_python")
class DefaultExtensionAddon(Addon):
    def on_create_instance(
        self, ten_env: TenEnv, name: str, context: object
    ) -> None:
        ten_env.log(LogLevel.INFO, "on_create_instance" + name)

        # function_entry, power, multi, substract

        if "function_entry" in name:
            ten_env.on_create_instance_done(
                FunctionEntryExtension(name), context
            )
        elif "power" in name:
            ten_env.on_create_instance_done(PowerExtension(name), context)
        elif "multi" in name:
            ten_env.on_create_instance_done(MultiExtension(name), context)
        elif "substract" in name:
            ten_env.on_create_instance_done(SubstractExtension(name), context)
        else:
            assert False
