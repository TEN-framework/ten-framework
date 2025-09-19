#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("main_python")
class DiarizationControlAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import DiarizationControlExtension

        ten.on_create_instance_done(
            DiarizationControlExtension(addon_name), context
        )
