from ten_runtime import Addon, TenEnv, register_addon_as_extension
from typing_extensions import override

from .extension import SonioxASRExtension


@register_addon_as_extension("soniox_asr_python")
class SonioxASRExtensionAddon(Addon):
    @override
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("on_create_instance")
        ten_env.on_create_instance_done(SonioxASRExtension(name), context)
