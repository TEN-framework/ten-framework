# 腾讯 ASR 异步 Python 扩展

一个用于腾讯云自动语音识别（ASR）服务的Python扩展，提供实时语音转文字功能，支持完整的异步架构。

## 功能

- **完整异步支持**: 基于完整异步架构构建，提供高性能语音识别
- **实时流式处理**: 支持低延迟的实时音频流处理
- **多种结束模式**: 可配置的结束策略（断开连接、静音包、厂商定义）
- **音频录制**: 可选的音频录制功能，用于调试和分析
- **保活支持**: 可配置的连接保活机制
- **错误处理**: 全面的错误处理和详细日志记录
- **多语言支持**: 支持多种语言和地区
- **可配置日志**: 可调整的日志级别用于调试

## 配置

扩展需要以下配置参数：

### 必需参数

- `app_id`: 腾讯云ASR应用ID
- `secret_key`: 腾讯云ASR密钥
- `params`: ASR请求参数（语言、音频格式等）

### 可选参数

- `finalize_mode`: 结束策略
  - `disconnect`: 结束后断开连接
  - `mute_pkg`: 发送静音包
  - `vendor_defined`: 使用厂商定义策略（默认）
- `mute_pkg_duration_ms`: 静音包持续时间（默认：800毫秒）
- `dump`: 启用音频录制（默认：false）
- `dump_path`: 录制音频文件路径
- `keep_alive_interval`: 保活间隔（秒）
- `log_level`: 日志级别（默认："INFO"）

### 配置示例

```json
{
  "app_id": "your_app_id",
  "secret_key": "your_secret_key",
  "params": {
    "language": "zh-CN",
    "format": "pcm",
    "sample_rate": 16000
  },
  "finalize_mode": "vendor_defined",
  "dump": false,
  "log_level": "INFO"
}
```

## API

扩展实现了 `AsyncASRBaseExtension` 接口，提供以下关键方法：

### 核心方法

- `on_init()`: 初始化ASR客户端和配置
- `start_connection()`: 建立与腾讯ASR服务的连接
- `stop_connection()`: 关闭ASR服务连接
- `send_audio()`: 发送音频帧进行识别
- `finalize()`: 结束当前识别会话

### 事件处理器

- `on_asr_start()`: ASR会话开始时调用
- `on_asr_sentence_start()`: 新句子开始时调用
- `on_asr_sentence_change()`: 句子内容变化时调用
- `on_asr_sentence_end()`: 句子结束时调用
- `on_asr_complete()`: ASR会话完成时调用
- `on_asr_fail()`: ASR失败时调用
- `on_asr_error()`: ASR遇到错误时调用

## 依赖

- `typing_extensions`: 用于类型提示
- `pydantic`: 用于配置验证
- `websockets`: 用于WebSocket通信
- `pytest`: 用于测试（开发依赖）

## 开发

### 构建

扩展作为TEN Framework构建系统的一部分构建，无需额外的构建步骤。

### 单元测试

使用以下命令运行单元测试：

```bash
pytest tests/
```

扩展包含以下方面的综合测试：
- 配置验证
- 音频处理
- 错误处理
- 连接管理

## 使用

1. **安装**: 扩展随TEN Framework自动安装
2. **配置**: 设置腾讯云ASR凭据和参数
3. **集成**: 通过TEN Framework ASR接口使用扩展
4. **监控**: 检查日志进行调试和监控

## 错误处理

扩展通过以下方式提供详细的错误信息：
- 模块错误代码
- 厂商特定错误详情
- 全面的日志记录
- 优雅降级

## 性能

- **低延迟**: 针对实时处理优化
- **高吞吐量**: 高效的音频帧处理
- **内存高效**: 最小的内存占用
- **连接复用**: 在可能时保持持久连接

## 安全

- **凭据加密**: 敏感凭据在配置中加密
- **安全通信**: 使用安全WebSocket连接
- **输入验证**: 全面的输入验证和清理

## 故障排除

### 常见问题

1. **连接失败**: 检查app_id和secret_key配置
2. **音频质量问题**: 验证音频格式和采样率设置
3. **性能问题**: 调整缓冲区设置和结束模式
4. **日志问题**: 配置适当的日志级别

### 调试模式

在配置中设置 `dump: true` 启用调试模式以录制音频进行分析。

## 许可证

此扩展是TEN Framework的一部分，根据Apache License, Version 2.0授权。
