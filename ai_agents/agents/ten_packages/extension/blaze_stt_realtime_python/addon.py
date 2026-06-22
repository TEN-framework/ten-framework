from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import BlazeSTTRealtimeExtension


@register_addon_as_extension("blaze_stt_realtime_python")
class BlazeSTTRealtimeExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(
            BlazeSTTRealtimeExtension(addon_name), context
        )
