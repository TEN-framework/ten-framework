from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)
from .extension import XfyunDialectASRExtension

@register_addon_as_extension("xfyun_asr_dialect_python")
class XfyunDialectASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(XfyunDialectASRExtension(addon_name), context)
