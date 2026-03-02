#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖 addon.py 的注册副作用
 * [OUTPUT]: 包导入时自动触发 siliconflow_tts2_python 的 addon 注册
 * [POS]: siliconflow_tts2_python 包入口，适配 Python addon loader 的导入约定
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

from . import addon
