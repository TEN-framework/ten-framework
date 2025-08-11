from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)
from .extension import XfyunASRExtension

@register_addon_as_extension("xfyun_asr_python")
class XfyunASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(XfyunASRExtension(addon_name), context)
