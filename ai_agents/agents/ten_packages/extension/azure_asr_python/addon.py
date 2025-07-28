from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("azure_asr_python")
class AzureASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import AzureASRExtension

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(AzureASRExtension(addon_name), context)
