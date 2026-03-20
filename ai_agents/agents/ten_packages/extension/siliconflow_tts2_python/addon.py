#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖 ten_runtime 的 Addon/注册器，依赖 extension.py 的 SiliconFlowTTSExtension
 * [OUTPUT]: 对外提供 siliconflow_tts2_python 扩展注册入口
 * [POS]: siliconflow_tts2_python 模块的 TEN 入口，被运行时按 addon 名称实例化
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

from ten_runtime import Addon, TenEnv, register_addon_as_extension

from .extension import SiliconFlowTTSExtension


@register_addon_as_extension("siliconflow_tts2_python")
class SiliconFlowTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info("SiliconFlowTTSExtensionAddon on_create_instance")
        ten_env.on_create_instance_done(
            SiliconFlowTTSExtension(name), context
        )

