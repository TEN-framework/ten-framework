"""Register the Rime realtime provider."""

from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("rime_mllm_python")
class RimeRealtimeAddon(Addon):
    """Create an independent provider session for each TEN extension instance."""

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import RimeRealtimeExtension

        ten_env.on_create_instance_done(RimeRealtimeExtension(name), context)
