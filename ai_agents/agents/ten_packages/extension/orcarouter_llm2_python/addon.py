#
#
# Agora Real Time Engagement
# OrcaRouter LLM2 Integration
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("orcarouter_llm2_python")
class OrcaRouterLLM2ExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import OrcaRouterLLM2Extension

        ten_env.log_info("OrcaRouterLLM2ExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(OrcaRouterLLM2Extension(name), context)
