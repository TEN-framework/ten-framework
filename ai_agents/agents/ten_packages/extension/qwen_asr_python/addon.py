from ten_runtime import Addon, TenEnv, register_addon_as_extension

from .extension import QwenASRExtension


@register_addon_as_extension("qwen_asr_python")
class QwenASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.on_create_instance_done(QwenASRExtension(addon_name), context)
