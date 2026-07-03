from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)
from .extension import ConversationRecorderExtension


@register_addon_as_extension("conversation_recorder")
class ConversationRecorderExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info(
            "ConversationRecorderExtensionAddon on_create_instance"
        )
        ten_env.on_create_instance_done(
            ConversationRecorderExtension(name), context
        )
