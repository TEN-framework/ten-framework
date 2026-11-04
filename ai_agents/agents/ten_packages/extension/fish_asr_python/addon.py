from ten_runtime import Addon, TenEnv, register_addon_as_extension
from typing_extensions import override

from .extension import FishAudioASRExtension


@register_addon_as_extension("fish_asr_python")
class FishAudioASRExtensionAddon(Addon):
    """Register the Fish Audio ASR extension with TEN."""

    @override
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("on_create_instance")
        ten_env.on_create_instance_done(FishAudioASRExtension(name), context)
