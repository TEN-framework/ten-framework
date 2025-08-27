from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("default_llm_extension_python")
class DefaultLLMExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import DefaultLLMExtension

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(DefaultLLMExtension(addon_name), context)
