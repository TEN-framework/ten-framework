#
#
# Agora Real Time Engagement
# Created by Wei Hu in 2024-08.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("stepfun_mllm_python")
class StepFunRealtime2ExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import StepFunRealtime2Extension

        ten_env.log_info("StepFunRealtime2ExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(
            StepFunRealtime2Extension(name), context
        )
