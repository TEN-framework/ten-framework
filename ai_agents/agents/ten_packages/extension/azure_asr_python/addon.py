from typing_extensions import override
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import AzureASRExtension


@register_addon_as_extension("azure_asr_python")
class AzureASRExtensionAddon(Addon):
    @override
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("on_create_instance")
        ten_env.on_create_instance_done(AzureASRExtension(name), context)
