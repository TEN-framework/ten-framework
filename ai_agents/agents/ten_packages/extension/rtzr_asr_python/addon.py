from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("rtzr_asr_python")
class RTZRASRAddon(Addon):
    def on_create_instance(self, ten: TenEnv, name: str, context) -> None:
        from .extension import RTZRASRExtension

        ten.on_create_instance_done(RTZRASRExtension(name), context)
