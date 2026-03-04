---
title: 基于WebSocket的语音助手快速入门
_portal_target: getting-started/websocket-voice-assistant-quick-start.cn.md
---

## 基于 WebSocket 的语音助手快速入门

在不到 10 分钟内构建并运行一个基于 WebSocket 的实时语音助手。

## 简介

本指南将帮助您快速运行一个完整的语音助手 Demo，支持实时语音对话和工具调用。该示例集成了：

| 组件 | 服务商     | 用途             |
| ---- | ---------- | ---------------- |
| ASR  | Deepgram   | 语音转文字       |
| LLM  | OpenAI     | 对话生成         |
| TTS  | ElevenLabs | 文字转语音       |
| Tool | WeatherAPI | 天气查询（可选） |

## 环境要求

### 前置依赖

请先参考 [TEN Framework 快速入门指南](./quick-start.cn.md) 完成基础环境安装，确保以下命令通过检查：

```bash
tman check env
```

预期输出（至少 Python 和 Go 为 Ready）：

```text
✅ Operating System: Supported
✅ Python:   Ready
✅ Go:       Ready
```

### 额外工具

安装以下工具用于构建和运行：

#### Task（任务运行器）

**Linux / macOS：**

```bash
go install github.com/go-task/task/v3/cmd/task@latest
task --version  # 验证安装
```

**Windows：**

```powershell
# 1. 使用winget安装（推荐）
# 从以下地址安装winget
# https://apps.microsoft.com/detail/9nblggh4nns1?hl=en-US&gl=US
# 系统需要满足：Windows10 高于 1709 (Build 16299)，或者是Windows11
winget --version # 验证安装
winget install Task.Task
task --version  # 验证安装

# 2. 使用scoop安装
irm get.scoop.sh | iex
scoop --version # 验证安装
scoop install task
task --version # 验证安装
```

#### Bun（JavaScript 包管理器）

**Linux / macOS：**

```bash
curl -fsSL https://bun.sh/install | bash
bun --version  # 验证安装
```

**Windows：**

```powershell
powershell -c "irm bun.sh/install.ps1 | iex"
bun --version  # 验证安装
```

#### uv（Python 包管理器）

**Linux / macOS：**

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv --version  # 验证安装
```

**Windows：**

```powershell
powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
uv --version  # 验证安装
```

> 💡 安装后需要重新加载终端配置：
>
> - Linux/macOS：`source ~/.zshrc`
> - Windows：重新打开 PowerShell 终端

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/TEN-framework/ten-framework.git
cd ten-framework/ai_agents/agents/examples/websocket-example
```

### 2. 安装依赖

```bash
task install
```

> ⏱️ 预计耗时：2-3 分钟

该命令会自动完成：

- 安装 TEN 扩展包（45+ 个）
- 编译 Go 应用和 API 服务器
- 安装 Python 和前端依赖

### 3. 配置 API Keys

在 `ai_agents/.env` 文件中配置服务商密钥：

**Linux / macOS：**

```bash
cd ../../../  # 回到 ai_agents 目录
vim .env
```

**Windows：**

```powershell
cd ..\..\..   # 回到 ai_agents 目录
notepad .env
```

添加以下配置：

```bash
# Deepgram - 语音识别
DEEPGRAM_API_KEY=your_deepgram_api_key

# OpenAI - 大语言模型
OPENAI_API_KEY=your_openai_api_key
OPENAI_MODEL=your_openai_model

# ElevenLabs - 语音合成
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key

# WeatherAPI - 天气工具（可选）
WEATHERAPI_API_KEY=your_weatherapi_key
```

### 4. 启动服务

```bash
cd agents/examples/websocket-example
task run
```

服务启动后，自动运行：

- **前端 UI**：<http://localhost:3000>
- **API 服务器**：<http://localhost:8080>
- **TMAN Designer**：<http://localhost:49483>

### 5. 与助手对话

打开浏览器访问 [http://localhost:3000](http://localhost:3000)，点击麦克风按钮开始语音对话。

尝试以下对话：

- "你好，你是谁？"
- "今天北京的天气怎么样？"（需配置 WeatherAPI）
- "给我讲个笑话"

**🎉 恭喜！** 您已成功运行 TEN Agent 语音助手。

## 进阶配置

### 更换扩展

TEN Framework 支持灵活替换 ASR、LLM、TTS 等扩展。

#### 方式 1：使用 TMAN Designer（推荐）

访问 [http://localhost:49483](http://localhost:49483)，通过可视化界面：

1. 点击扩展节点查看详情
2. 选择替代扩展
3. 配置参数
4. 保存并重启

#### 方式 2：编辑配置文件

编辑 `tenapp/property.json`，修改 `predefined_graphs` 中的扩展配置。

**示例：更换 ASR 为 Azure**

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "azure_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "key": "${env:AZURE_STT_KEY}",
      "region": "${env:AZURE_STT_REGION}",
      "language": "en-US"
    }
  }
}
```

**示例：更换 TTS 为 Minimax**

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "minimax_tts_websocket_python",
  "extension_group": "tts",
  "property": {
    "params": {
      "api_key": "${env:MINIMAX_TTS_API_KEY|}",
      "group_id": "${env:MINIMAX_TTS_GROUP_ID|}",
      "model": "speech-02-turbo",
      "audio_setting": {
        "sample_rate": 16000
      },
      "voice_setting": {
        "voice_id": "female-shaonv"
      }
    }
  }
}
```

修改后重启服务（`Ctrl+C` 然后 `task run`）。

### 配置监控

使用 Grafana 监控应用的 Metrics 和 Logs。

#### 1. 启动监控栈

**Linux / macOS：**

```bash
cd tools/grafana-monitoring
docker compose -f docker-compose.push.yml up -d
```

**Windows：**

```powershell
cd tools\grafana-monitoring
docker compose -f docker-compose.push.yml up -d
# 若docker未安装，请通过https://apps.microsoft.com/detail/xp8cbj40xlbwkx 安装，并重启电脑后，启动docker desktop
```

启动的服务：

- **Grafana**（端口 3001）- 可视化界面
- **Prometheus**（端口 9091）- Metrics 存储
- **Loki**（端口 3100）- Logs 存储
- **OpenTelemetry Collector**（端口 4317/4318）- OTLP 数据接收

#### 2. 配置应用遥测

编辑 `agents/examples/websocket-example/tenapp/property.json`：

**配置 Logs 输出**

在 `ten.log.handlers` 数组中添加：

```json
{
  "matchers": [{ "level": "info" }],
  "formatter": { "type": "json", "colored": false },
  "emitter": {
    "type": "otlp",
    "config": {
      "endpoint": "http://localhost:4317",
      "service_name": "websocket-example"
    }
  }
}
```

#### 配置 Metrics 输出

在 `ten.services` 中添加：

```json
{
  "telemetry": {
    "enabled": true,
    "metrics": {
      "enabled": true,
      "exporter": {
        "type": "otlp",
        "config": {
          "endpoint": "http://localhost:4317",
          "service_name": "websocket-example"
        }
      }
    }
  }
}
```

#### 3. 重启并访问 Grafana

```bash
cd ../../../agents/examples/websocket-example
task run
```

访问 [http://localhost:3001](http://localhost:3001)（用户名/密码：`admin/admin`）

#### 查看 Metrics

进入 `Dashboards` → **"TEN Framework"** 查看：

- 扩展启动耗时
- Cmd 处理延迟
- 消息队列等待时间

#### 查看 Logs

进入 `Explore` → 选择 `Loki`，使用查询：

- `{service_name="websocket-example"}` - 所有日志
- `{service_name="websocket-example", ten_extension_name="stt"}` - ASR 日志
- `{service_name="websocket-example", ten_extension_name="llm"}` - LLM 日志
- `{service_name="websocket-example", ten_extension_name="tts"}` - TTS 日志

> 💡 使用 `ten_extension_name` 标签精确筛选扩展日志。

停止监控服务：

**Linux / macOS：**

```bash
cd tools/grafana-monitoring
docker compose -f docker-compose.push.yml down
```

**Windows：**

```powershell
cd tools\grafana-monitoring
docker compose -f docker-compose.push.yml down
```

## 常见问题

<details>
<summary><strong>前端依赖安装失败</strong></summary>

如果 `bun install` 报错版本不存在，切换到 npm 官方源：

```bash
cd frontend
echo "registry=https://registry.npmjs.org/" > .npmrc
rm bun.lock
bun install
```

</details>

<details>
<summary><strong>macOS Python 库加载失败</strong></summary>

设置 Python 动态库路径：

```bash
export DYLD_LIBRARY_PATH=/usr/local/opt/python@3.10/Frameworks/Python.framework/Versions/3.10/lib:$DYLD_LIBRARY_PATH
# 添加到 ~/.zshrc 永久生效
```

</details>

<details>
<summary><strong>Windows Python 库加载失败</strong></summary>

确保 Python 3.10 安装路径在系统 PATH 中：

```powershell
# 检查 Python 路径
python -c "import sys; print(sys.prefix)"

# 将 Python DLL 目录添加到 PATH（根据实际路径调整）
$env:Path += ";C:\Python310;C:\Python310\DLLs"
```

如果安装 Python 时未勾选 "Add Python to PATH"，可以重新运行安装程序选择 "Modify" 来添加。

</details>

<details>
<summary><strong>端口被占用</strong></summary>

查找并终止占用端口的进程：

**Linux / macOS：**

```bash
lsof -i :3000  # 或 :8080
kill -9 <PID>
```

**Windows：**

```powershell
netstat -ano | findstr :3000   # 或 :8080
taskkill /PID <PID> /F
```

</details>

## 下一步

跟随这些指南，将您的语音 AI 应用带入真实世界。

- **[扩展开发指南](https://theten.ai/cn/docs/ten_framework/development/how_to_develop_with_ext)** - 开发自定义扩展
- **[更多示例](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples)** - 探索其他 Agent 示例
- **[GitHub Issues](https://github.com/TEN-framework/ten-framework/issues)** - 报告问题或请求功能
- **[文档中心](https://theten.ai/cn/docs)** - 完整的 TEN Framework 文档
