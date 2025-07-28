from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("transcribe_asr_python")
class TranscribeAsrExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import TranscribeASRExtension

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(TranscribeASRExtension(addon_name), context)
