from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("fpt_mllm_python")
class FPTRealtimeExtensionAddon(Addon):
    def on_create_instance(
        self, ten_env: TenEnv, name: str, context
    ) -> None:
        from .extension import FPTRealtimeExtension

        ten_env.log_info("FPTRealtimeExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(FPTRealtimeExtension(name), context)

