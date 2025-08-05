# AWS ASR Python 扩展

一个用于 AWS 自动语音识别 (ASR) 服务的 Python 扩展，提供实时语音转文本转换功能，完全支持异步操作，使用 AWS Transcribe 流式 API。

## 功能特性

- **完全异步支持**: 采用完整的异步架构，实现高性能语音识别
- **实时流式处理**: 使用 AWS Transcribe 的流式 API 支持低延迟实时音频流
- **AWS Transcribe API**: 使用 AWS Transcribe 流式转录 API，提供企业级性能
- **多种音频格式**: 支持 PCM16 音频格式
- **音频转储**: 可选的音频录制功能，用于调试和分析
- **可配置日志**: 可调整的日志级别，便于调试
- **错误处理**: 全面的错误处理和详细日志记录
- **多语言支持**: 通过 AWS Transcribe 支持多种语言
- **重连管理**: 自动重连机制，确保服务稳定性
- **会话管理**: 支持会话 ID 和音频时间线管理

## 配置

扩展需要以下配置参数：

### 必需参数

- `params`: AWS Transcribe 配置参数，包括认证信息和转录设置

### 可选参数

- `dump`: 启用音频转储（默认：false）
- `dump_path`: 转储音频文件的路径（默认："aws_asr_in.pcm"）
- `log_level`: 日志级别（默认："INFO"）
- `finalize_mode`: 完成模式，可选 "disconnect" 或 "mute_pkg"（默认："disconnect"）
- `mute_pkg_duration_ms`: 静音包持续时间（毫秒）（默认：800）

### AWS Transcribe 配置参数

- `region`: AWS 区域，例如 'us-west-2'
- `access_key_id`: AWS 访问密钥 ID
- `secret_access_key`: AWS 秘密访问密钥
- `language_code`: 语言代码，例如 'en-US', 'zh-CN'
- `media_sample_rate_hz`: 音频采样率（Hz），例如 16000
- `media_encoding`: 音频编码格式，例如 'pcm'
- `vocabulary_name`: 自定义词汇表名称（可选）
- `session_id`: 会话 ID（可选）
- `vocab_filter_method`: 词汇过滤方法（可选）
- `vocab_filter_name`: 词汇过滤器名称（可选）
- `show_speaker_label`: 是否显示说话人标签（可选）
- `enable_channel_identification`: 是否启用声道识别（可选）
- `number_of_channels`: 声道数量（可选）
- `enable_partial_results_stabilization`: 是否启用部分结果稳定化（可选）
- `partial_results_stability`: 部分结果稳定性设置（可选）
- `language_model_name`: 语言模型名称（可选）

### 配置示例

```json
{
  "params": {
    "region": "us-west-2",
    "access_key_id": "your_aws_access_key_id",
    "secret_access_key": "your_aws_secret_access_key",
    "language_code": "en-US",
    "media_sample_rate_hz": 16000,
    "media_encoding": "pcm",
    "vocabulary_name": "custom-vocabulary",
    "show_speaker_label": true,
    "enable_partial_results_stabilization": true,
    "partial_results_stability": "HIGH"
  },
  "dump": false,
  "log_level": "INFO",
  "finalize_mode": "disconnect",
  "mute_pkg_duration_ms": 800
}
```

## API

扩展实现了 `AsyncASRBaseExtension` 接口，提供以下关键方法：

### 核心方法

- `on_init()`: 初始化 AWS ASR 客户端和配置
- `start_connection()`: 建立与 AWS Transcribe 服务的连接
- `stop_connection()`: 关闭与 ASR 服务的连接
- `send_audio()`: 发送音频帧进行识别
- `finalize()`: 完成当前识别会话
- `is_connected()`: 检查连接状态

### 事件处理器

- `on_asr_start()`: ASR 会话开始时调用
- `on_asr_delta()`: 收到转录增量时调用
- `on_asr_completed()`: 转录完成时调用
- `on_asr_committed()`: 音频缓冲区提交时调用
- `on_asr_server_error()`: 服务器错误时调用
- `on_asr_client_error()`: 客户端错误时调用

### 内部方法

- `_handle_transcript_event()`: 处理转录事件
- `_disconnect_aws()`: 断开 AWS 连接
- `_reconnect_aws()`: 重新连接 AWS
- `_handle_finalize_disconnect()`: 处理断开连接完成
- `_handle_finalize_mute_pkg()`: 处理静音包完成

## 依赖项

- `typing_extensions`: 用于类型提示
- `pydantic`: 用于配置验证和数据模型
- `amazon-transcribe`: AWS Transcribe Python 客户端库
- `pytest`: 用于测试（开发依赖）

## 开发

### 构建

扩展作为 TEN Framework 构建系统的一部分进行构建。无需额外的构建步骤。

### 测试

运行单元测试：

```bash
pytest tests/
```

扩展包含全面的测试：
- 配置验证
- 音频处理
- 错误处理
- 连接管理
- 转录结果处理

## 使用方法

1. **安装**: 扩展随 TEN Framework 自动安装
2. **配置**: 设置您的 AWS 凭据和 Transcribe 参数
3. **集成**: 通过 TEN Framework ASR 接口使用扩展
4. **监控**: 检查日志以进行调试和监控

## 错误处理

扩展通过以下方式提供详细的错误信息：
- 模块错误代码
- AWS 特定错误详情
- 全面的日志记录
- 优雅降级和重连机制

## 性能

- **低延迟**: 使用 AWS Transcribe 的流式 API 优化实时处理
- **高吞吐量**: 高效的音频帧处理
- **内存高效**: 最小的内存占用
- **连接复用**: 维护持久的连接
- **自动重连**: 网络中断时自动重连

## 安全性

- **凭据加密**: 敏感凭据在配置中加密
- **安全通信**: 使用与 AWS 的安全连接
- **输入验证**: 全面的输入验证和清理
- **IAM 权限**: 支持 AWS IAM 权限管理

## 支持的 AWS 功能

扩展支持各种 AWS Transcribe 功能：
- **多语言支持**: 支持多种语言和方言
- **自定义词汇表**: 支持自定义词汇表
- **词汇过滤**: 支持词汇过滤功能
- **说话人识别**: 支持说话人标签
- **声道识别**: 支持多声道音频处理
- **部分结果**: 支持实时部分结果
- **结果稳定化**: 支持结果稳定化设置

## 音频格式支持

- **PCM16**: 16 位 PCM 音频格式
- **采样率**: 支持多种采样率（如 16000 Hz）
- **单声道**: 支持单声道音频处理

## 故障排除

### 常见问题

1. **连接失败**: 检查 AWS 凭据和网络连接
2. **认证错误**: 验证 AWS 访问密钥和权限
3. **音频质量问题**: 验证音频格式和采样率设置
4. **性能问题**: 调整缓冲区设置和语言模型
5. **日志问题**: 配置适当的日志级别

### 调试模式

通过在配置中设置 `dump: true` 启用调试模式，以录制音频进行分析。

### 重连机制

扩展包含自动重连机制：
- 网络中断时自动重连
- 可配置的重连策略
- 连接状态监控

## 许可证

此扩展是 TEN Framework 的一部分，根据 Apache License, Version 2.0 授权。 
