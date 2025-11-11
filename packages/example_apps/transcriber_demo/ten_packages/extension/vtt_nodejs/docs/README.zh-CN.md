# VTT 录制扩展

一个基于 Node.js/TypeScript 的 TEN Framework 扩展，用于录制音频流和 ASR（自动语音识别）结果，生成标准的 WebVTT 字幕文件。

## 功能特性

- **音频录制**：将 PCM 音频帧录制为 WAV 文件
- **VTT 生成**：从 ASR 结果创建 WebVTT 字幕文件
- **会话管理**：管理多个录制会话及元数据
- **多格式导出**：以 VTT、JSON 和 WAV 格式保存转录内容
- **实时处理**：实时处理流式音频和文本数据

## 架构设计

```text
┌─────────────────┐
│   音频源        │ (例如：音频播放器、麦克风)
└────────┬────────┘
         │ pcm_frame
         ↓
┌─────────────────┐
│                 │
│  VTT 录制器     │ ← asr_result ← ASR 引擎
│   (Node.js)     │
│                 │
└────────┬────────┘
         │
         ↓
   ┌─────────────────┐
   │  本地存储       │
   ├─────────────────┤
   │ • audio.wav     │
   │ • transcript.vtt│
   │ • metadata.json │
   └─────────────────┘
```

## 命令接口

### `start_recording`

开始一个新的录制会话。

**返回值：**

- `session_id`：会话的唯一标识符
- `detail`：状态消息

### `stop_recording`

停止当前录制会话并保存所有文件。

**返回值：**

- `session_id`：会话标识符
- `duration`：录制时长（毫秒）
- `segments`：字幕片段数量
- `words`：总词数

### `list_sessions`

列出所有录制会话。

**返回值：**

- `sessions`：会话元数据的 JSON 数组
- `count`：会话数量

### `get_session`

获取特定会话的元数据。

**参数：**

- `session_id`：会话标识符

**返回值：**

- `metadata`：包含会话详情的 JSON 对象

### `delete_session`

删除录制会话及其所有文件。

**参数：**

- `session_id`：会话标识符

## 输入数据

### 音频帧

- **名称**：`pcm_frame`
- **格式**：PCM 音频数据（推荐 16kHz，单声道，16位）
- **用法**：会话激活时自动录制

### ASR 结果

- **名称**：`asr_result`
- **属性**：
  - `text` (string)：转录文本
  - `final` (bool)：是否为最终结果
- **用法**：自动处理并转换为 VTT 片段

## 输出文件

每个录制会话创建一个目录，包含以下文件：

```text
recordings/
└── <session-id>/
    ├── audio.wav          # 录制的音频
    ├── transcript.vtt     # WebVTT 字幕文件
    ├── transcript.json    # JSON 格式转录
    └── metadata.json      # 会话元数据
```

### WebVTT 格式

```vtt
WEBVTT

1
00:00:00.000 --> 00:00:05.000
你好，这是一个测试。

2
00:00:05.000 --> 00:00:10.000
快速的棕色狐狸跳过了懒狗。
```

## 配置

编辑 `property.json` 进行自定义：

```json
{
  "recordings_path": "./recordings"
}
```

## 依赖项

- `node-wav`：WAV 文件编码（19KB）
- `uuid`：会话 ID 生成
- `ten-runtime-nodejs`：TEN Framework 运行时

## 安装

```bash
cd ten_packages/extension/vtt_nodejs
npm install
npm run build
```

## 使用示例

### 与 TEN Framework 集成

在应用的 `property.json` 中添加：

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "nodes": [
          {
            "type": "extension",
            "name": "vtt_recorder",
            "addon": "vtt_nodejs"
          }
        ],
        "connections": [
          {
            "extension": "audio_source",
            "audio_frame": [
              {
                "name": "pcm_frame",
                "dest": [{"extension": "vtt_recorder"}]
              }
            ]
          },
          {
            "extension": "asr_engine",
            "data": [
              {
                "name": "asr_result",
                "dest": [{"extension": "vtt_recorder"}]
              }
            ]
          }
        ]
      }
    ]
  }
}
```

### 控制录制

```typescript
// 开始录制
await tenEnv.sendCmd("start_recording");

// ... 音频和 ASR 数据自动流转 ...

// 停止录制
await tenEnv.sendCmd("stop_recording");
```

## 浏览器播放

在 HTML 中使用生成的文件：

```html
<audio controls>
  <source src="/recordings/<session-id>/audio.wav" type="audio/wav">
  <track src="/recordings/<session-id>/transcript.vtt"
         kind="subtitles"
         srclang="zh"
         label="中文">
</audio>
```

## 技术细节

### 音频处理

- 支持多种采样率（自动检测）
- 支持 8/16/24/32 位 PCM 格式
- 转换为 16 位 WAV 以保证兼容性
- 使用流式处理最小化内存占用

### VTT 生成

- 自动句子分段
- 最大片段时长：7 秒
- 最小片段时长：1 秒
- 文本格式化（大写、标点）
- 与音频时间戳同步

### 会话管理

- 基于 UUID 的会话 ID
- 自动元数据追踪
- 安全的并发会话处理
- 错误时自动清理

## 性能

- **内存**：流式处理保持恒定内存使用
- **延迟**：每个音频帧 < 10ms
- **存储**：16kHz 单声道音频约 32 KB/s
- **CPU**：最小开销（< 5%）

## 故障排除

### 录制无法开始

- 检查是否有其他会话正在活动
- 验证音频源是否发送 `pcm_frame` 数据

### VTT 文件为空

- 确保 ASR 结果包含 `final=true`
- 检查 ASR 是否发送到正确的扩展

### 文件过大

- 默认 WAV 格式未压缩
- 如需要可考虑后处理压缩

## 许可证

Apache License 2.0

## 支持

如有问题和疑问，请参考 TEN Framework 文档。
