from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)
from .extension import AliyunASRBigmodelExtension


@register_addon_as_extension("aliyun_asr_bigmodel_python")
class AliyunASRBigmodelExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:

        ten.log_info("on_create_instance")
        ten.on_create_instance_done(
            AliyunASRBigmodelExtension(addon_name), context
        )
