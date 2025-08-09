from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import DeepgramASRExtension

@register_addon_as_extension("deepgram_asr_python")
class DeepgramASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(DeepgramASRExtension(addon_name), context)
