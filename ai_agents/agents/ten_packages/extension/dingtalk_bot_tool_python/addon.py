"""
DingTalk Bot Addon Registration
"""

from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)

from .extension import DingTalkBotExtension


@register_addon_as_extension("dingtalk_bot_tool_python")
class DingTalkBotAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("Creating DingTalk Bot Extension instance")
        ten_env.on_create_instance_done(DingTalkBotExtension(name), context)
