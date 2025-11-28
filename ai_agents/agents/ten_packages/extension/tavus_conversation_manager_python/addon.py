from ten_runtime import Addon, register_addon_as_extension, TenEnv
from .extension import TavusConversationManagerExtension


@register_addon_as_extension("tavus_conversation_manager_python")
class TavusConversationManagerExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        ten.log_info("TavusConversationManagerExtensionAddon on_create_instance")
        ten.on_create_instance_done(
            TavusConversationManagerExtension(addon_name), context
        )
