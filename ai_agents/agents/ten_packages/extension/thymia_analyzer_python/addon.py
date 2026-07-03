#
# Agora Real Time Engagement
# Created by Claude Code in 2025-10.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("thymia_analyzer_python")
class ThymiaAnalyzerExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import ThymiaAnalyzerExtension

        ten_env.log_info("ThymiaAnalyzerExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(ThymiaAnalyzerExtension(name), context)
