from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("default_asr_extension_python")
class DefaultASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import DefaultASRExtension

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(DefaultASRExtension(addon_name), context)
