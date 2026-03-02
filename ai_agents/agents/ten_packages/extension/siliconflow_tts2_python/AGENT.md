# siliconflow_tts2_python/
> L2 | 父级: /mnt/e/wsf-project/ai_agents/AGENTS.md

成员清单
`__init__.py`: 包声明，保持 Python 扩展目录可导入。
`addon.py`: TEN addon 注册入口，暴露 `siliconflow_tts2_python` 扩展实例。
`config.py`: SiliconFlow TTS 配置模型，归一化默认参数并校验采样率与响应格式。
`extension.py`: HTTP TTS 基座适配层，负责创建配置/客户端并暴露采样率。
`siliconflow_tts.py`: 供应商 HTTP 客户端，请求 `/audio/speech`，嗅探真实响应格式，并把 MPEG 解码成 PCM 数据块。
`wav_stream_parser.py`: 流式 WAV 头解析器，仅在响应真实为 RIFF/WAV 时拆出 PCM 数据。
`manifest.json`: 扩展元数据与属性模式，供 tman 和 TEN 运行时读取。
`property.json`: 默认属性模板，约定 SiliconFlow 的环境变量和默认音色。
`requirements.txt`: Python 依赖声明，包含 `httpx` 与 `miniaudio`。
`README.md`: 扩展说明与最小配置示例。

法则: 成员完整·一行一文件·父级链接·技术词前置

[PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
