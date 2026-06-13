from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("xai_tts_python")
class XAITTSExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import XAITTSExtension

        ten_env.log_info("XAITTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(XAITTSExtension(name), context)
