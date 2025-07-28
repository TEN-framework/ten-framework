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


@register_addon_as_extension("stepfun_v2v_python")
class StepFunRealtimeExtensionAddon(Addon):

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import StepFunRealtimeExtension

        ten_env.log_info("StepFunRealtimeExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(StepFunRealtimeExtension(name), context)
