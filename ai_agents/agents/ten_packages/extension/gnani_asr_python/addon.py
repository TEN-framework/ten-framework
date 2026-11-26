from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("gnani_asr_python")
class GnaniASRAddon(Addon):
    def on_create_instance(self, ten: TenEnv, name: str, context) -> None:
        from .extension import GnaniASRExtension

        ten.on_create_instance_done(GnaniASRExtension(name), context)
