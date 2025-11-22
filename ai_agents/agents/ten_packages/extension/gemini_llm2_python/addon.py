#
#
# Agora Real Time Engagement
# Created for Google Gemini Integration
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("gemini_llm2_python")
class GeminiLLM2ExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import GeminiLLM2Extension

        ten_env.log_info("GeminiLLM2ExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(GeminiLLM2Extension(name), context)
