from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import DeepgramWSASRExtension


@register_addon_as_extension("deepgram_ws_asr_python")
class DeepgramWSASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(DeepgramWSASRExtension(addon_name), context)
