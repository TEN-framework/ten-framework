from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import SmallestASRExtension


@register_addon_as_extension("smallest_asr_python")
class SmallestASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(SmallestASRExtension(addon_name), context)
