from ten_runtime import Addon, register_addon_as_extension, TenEnv
from .extension import OracleASRExtension


@register_addon_as_extension("oracle_asr_python")
class OracleASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(OracleASRExtension(addon_name), context)
