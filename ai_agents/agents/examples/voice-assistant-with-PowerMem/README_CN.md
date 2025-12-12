# 语音助手 (集成 PowerMem)

一个集成了 [PowerMem](https://github.com/oceanbase/powermem/) 记忆管理能力的语音助手，支持持久化的对话上下文。

> 📖 **English Documentation**: [README.md](./README.md)

## 📋 目录

- [前置条件](#前置条件)
- [快速开始 (Docker)](#快速开始-docker-推荐)
- [本地开发](#本地开发)
- [配置指南](#配置指南)
- [验证部署](#验证部署)
- [故障排除](#故障排除)

## 前置条件

在开始之前，请确保您已具备以下条件：

### 系统要求

- **Docker** (版本 20.10+) 和 **Docker Compose** (版本 2.0+)
  - 检查命令：`docker --version` 和 `docker-compose --version`
- **Git** 用于克隆仓库
- 至少 **4GB RAM** 可用内存供容器使用

### API 账号和密钥

您需要创建账号并从以下服务获取 API 密钥：

| 服务 | 用途 | 获取地址 |
|------|------|----------|
| **Agora** | 实时通信 | [Agora 控制台](https://console.agora.io/) |
| **Deepgram** | 语音转文字 (ASR) | [Deepgram 控制台](https://console.deepgram.com/) |
| **OpenAI** | 语言模型 (LLM) | [OpenAI 平台](https://platform.openai.com/) |
| **ElevenLabs** | 文字转语音 (TTS) | [ElevenLabs](https://elevenlabs.io/) |
| **Qwen** (或其他) | PowerMem LLM 和嵌入模型 | [DashScope](https://dashscope.aliyun.com/) |

> 💡 **提示**：请妥善保管您的 API 密钥。切勿将其提交到版本控制系统。

## 快速开始 (Docker - 推荐)

这是最简单的开始方式。Docker 会自动处理所有依赖项。

### 步骤 1：配置环境变量

1. 导航到项目根目录：
   ```bash
   cd /path/to/ten-framework/ai_agents
   ```

2. 编辑 `.env` 文件（如果不存在则创建）：
   ```bash
   # .env 文件位于：ai_agents/.env
   # 如果有 .env.example 文件，可以从中复制
   ```

3. 配置以下 **必需** 变量：

   **语音助手服务：**
   ```bash
   # Agora - 实时通信
   AGORA_APP_ID=your_agora_app_id
   AGORA_APP_CERTIFICATE=your_agora_certificate  # 可选但推荐

   # Deepgram - 语音识别
   DEEPGRAM_API_KEY=your_deepgram_api_key

   # OpenAI - 语言模型
   OPENAI_API_KEY=your_openai_api_key
   OPENAI_MODEL=gpt-4  # 可选，默认为配置的模型

   # ElevenLabs - 文字转语音
   ELEVENLABS_TTS_KEY=your_elevenlabs_api_key
   ```

   **PowerMem 配置：**
   ```bash
   # 时区
   TIMEZONE=Asia/Shanghai  # 根据您的时区调整

   # 数据库 (OceanBase 会由 docker-compose 自动启动)
   DATABASE_PROVIDER=oceanbase
   OCEANBASE_HOST=seekdb  # Docker 使用 'seekdb'，本地使用 '127.0.0.1'
   OCEANBASE_PORT=2881
   OCEANBASE_USER=root
   OCEANBASE_PASSWORD=  # SeekDB 默认留空
   OCEANBASE_DATABASE=powermem
   OCEANBASE_COLLECTION=memories

   # LLM 提供商 (用于 PowerMem)
   LLM_PROVIDER=qwen  # 选项：qwen, openai, siliconflow
   LLM_API_KEY=your_qwen_api_key
   LLM_MODEL=qwen-plus
   LLM_BASE_URL=https://dashscope.aliyuncs.com/api/v1

   # 嵌入模型提供商 (用于 PowerMem)
   EMBEDDING_PROVIDER=qwen  # 选项：qwen, openai, mock
   EMBEDDING_API_KEY=your_qwen_api_key  # 可以与 LLM_API_KEY 相同
   EMBEDDING_MODEL=text-embedding-v4
   EMBEDDING_DIMS=1536
   EMBEDDING_BASE_URL=https://dashscope.aliyuncs.com/api/v1
   ```

   **可选变量：**
   ```bash
   OPENAI_PROXY_URL=  # 可选：OpenAI API 代理
   WEATHERAPI_API_KEY=  # 可选：用于天气工具功能
   ```

### 步骤 2：启动服务

1. 导航到示例目录：
   ```bash
   cd ai_agents/agents/examples/voice-assistant-with-PowerMem
   ```

2. 使用 Docker Compose 启动所有服务：
   ```bash
   docker-compose up -d
   ```

   这将：
   - 构建语音助手容器
   - 启动 OceanBase (SeekDB) 数据库容器
   - 初始化 PowerMem 数据库架构
   - 启动 API 服务器和前端

3. 检查容器状态：
   ```bash
   docker-compose ps
   ```

   您应该看到两个正在运行的容器：
   - `voice-assistant-with-powermem` (主应用程序)
   - `seekdb` (OceanBase 数据库)

### 步骤 3：验证部署

1. **检查日志**以确保一切正常启动：
   ```bash
   docker-compose logs -f voice-assistant
   ```

   查找：
   - ✅ "Server started" 或类似的成功消息
   - ✅ 没有严重错误

2. **访问应用程序：**
   - **前端界面**：http://localhost:3000
   - **API 服务器**：http://localhost:8080

3. **测试 API**（可选）：
   ```bash
   curl http://localhost:8080/ping
   ```

### 步骤 4：停止服务（需要时）

```bash
docker-compose down
```

同时删除数据卷（数据库数据）：
```bash
docker-compose down -v
```

## 本地开发

如果您希望不使用 Docker 运行，请按照以下步骤操作：

### 步骤 1：安装依赖

1. 确保您已安装：
   - Python 3.8+
   - Node.js 20+
   - Go 1.21+
   - [Task](https://taskfile.dev/) 构建工具

2. 安装项目依赖：
   ```bash
   cd ai_agents/agents/examples/voice-assistant-with-PowerMem
   task install
   ```

### 步骤 2：配置环境

1. 编辑 `ai_agents/.env` 文件（与 Docker 设置相同）

2. **重要**：对于本地开发，更新 OceanBase 连接：
   ```bash
   OCEANBASE_HOST=127.0.0.1  # 使用 localhost 而不是 'seekdb'
   ```

3. **本地启动 OceanBase**（如果不使用 Docker）：
   ```bash
   # 选项 1：仅使用 Docker 运行数据库
   docker run -d --name seekdb \
     -p 2881:2881 -p 2886:2886 \
     -v $(pwd)/data:/var/lib/oceanbase \
     oceanbase/seekdb:latest

   # 选项 2：本地安装 OceanBase（参见 OceanBase 文档）
   ```

### 步骤 3：运行应用程序

```bash
task run
```

这将启动：
- **API 服务器**：http://localhost:8080
- **前端**：http://localhost:3000
- **TMAN Designer**：http://localhost:49483 (用于图形可视化)

## 配置指南

### 环境变量参考

#### 语音助手服务

| 变量 | 必需 | 描述 | 示例 |
|------|------|------|------|
| `AGORA_APP_ID` | ✅ 是 | Agora RTC 应用 ID | `abc123def456` |
| `AGORA_APP_CERTIFICATE` | ⚠️ 可选 | Agora 证书（生产环境推荐） | `cert123...` |
| `DEEPGRAM_API_KEY` | ✅ 是 | Deepgram ASR API 密钥 | `key_abc123...` |
| `OPENAI_API_KEY` | ✅ 是 | OpenAI LLM API 密钥 | `sk-...` |
| `OPENAI_MODEL` | ⚠️ 可选 | OpenAI 模型名称 | `gpt-4` |
| `ELEVENLABS_TTS_KEY` | ✅ 是 | ElevenLabs TTS API 密钥 | `abc123...` |

#### PowerMem 配置

**数据库：**
- `DATABASE_PROVIDER`: `oceanbase` (推荐), `sqlite`, 或 `postgres`
- `OCEANBASE_HOST`: `seekdb` (Docker) 或 `127.0.0.1` (本地)
- `OCEANBASE_PORT`: `2881` (默认)
- `OCEANBASE_USER`: `root` (默认)
- `OCEANBASE_PASSWORD`: SeekDB 默认留空
- `OCEANBASE_DATABASE`: `powermem` (默认)
- `OCEANBASE_COLLECTION`: `memories` (默认)

**LLM 提供商：**
- `LLM_PROVIDER`: `qwen` (推荐), `openai`, 或 `siliconflow`
- `LLM_API_KEY`: 您的提供商 API 密钥
- `LLM_MODEL`: 模型名称 (例如：`qwen-plus`, `gpt-4`)
- `LLM_BASE_URL`: API 基础 URL

**嵌入模型提供商：**
- `EMBEDDING_PROVIDER`: `qwen` (推荐), `openai`, 或 `mock`
- `EMBEDDING_API_KEY`: 您的提供商 API 密钥
- `EMBEDDING_MODEL`: 模型名称 (例如：`text-embedding-v4`)
- `EMBEDDING_DIMS`: 嵌入维度 (例如：`1536`)

### 替代 LLM/嵌入模型提供商

**使用 OpenAI 作为 PowerMem：**
```bash
LLM_PROVIDER=openai
LLM_API_KEY=${OPENAI_API_KEY}  # 复用 OpenAI 密钥
LLM_MODEL=gpt-4
LLM_BASE_URL=https://api.openai.com/v1

EMBEDDING_PROVIDER=openai
EMBEDDING_API_KEY=${OPENAI_API_KEY}
EMBEDDING_MODEL=text-embedding-3-large
EMBEDDING_DIMS=3072
```

## 验证部署

### 1. 检查容器健康状态

```bash
# 检查容器是否运行
docker-compose ps

# 检查日志中的错误
docker-compose logs voice-assistant | tail -50
docker-compose logs seekdb | tail -50
```

### 2. 测试 API 端点

```bash
# 健康检查
curl http://localhost:8080/ping

# 应该返回成功响应
```

### 3. 访问前端

1. 在浏览器中打开 http://localhost:3000
2. 您应该看到语音助手界面
3. 尝试开始对话以验证端到端功能

### 4. 验证数据库连接

```bash
# 检查 OceanBase 是否可访问
docker exec -it seekdb bash
# 在容器内：
mysql -h127.0.0.1 -P2881 -uroot -e "SHOW DATABASES;"
```

## 故障排除

### 常见问题

#### 1. 容器无法启动

**问题**：`docker-compose up` 失败或容器立即退出

**解决方案**：
- 检查 Docker 是否运行：`docker ps`
- 检查端口冲突：`netstat -tuln | grep -E '3000|8080|2881'`
- 查看详细日志：`docker-compose logs`
- 确保 `.env` 文件存在且包含所有必需的变量

#### 2. 数据库连接被拒绝

**问题**：语音助手无法连接到 OceanBase

**解决方案**：
- 验证 `OCEANBASE_HOST` 是否正确：
  - Docker：使用 `seekdb`
  - 本地：使用 `127.0.0.1`
- 检查 SeekDB 容器是否运行：`docker-compose ps seekdb`
- 启动后等待几秒钟 - 数据库需要时间初始化
- 检查数据库日志：`docker-compose logs seekdb`

#### 3. API 密钥不工作

**问题**：API 服务返回认证错误

**解决方案**：
- 验证 `.env` 文件中的 API 密钥是否正确
- 检查值周围是否有额外的空格或引号
- 确保 API 密钥有足够的额度/配额
- 使用服务提供商的文档直接测试 API 密钥

#### 4. 前端无法加载

**问题**：http://localhost:3000 显示错误或空白页

**解决方案**：
- 检查前端容器日志：`docker-compose logs voice-assistant | grep -i frontend`
- 验证端口 3000 未被占用：`lsof -i :3000`
- 尝试直接访问 API：`curl http://localhost:8080/ping`
- 重建容器：`docker-compose up -d --build`

#### 5. PowerMem 初始化错误

**问题**：记忆功能不工作

**解决方案**：
- 验证所有 PowerMem 环境变量是否已设置
- 检查数据库连接（参见问题 #2）
- 确保 LLM 和嵌入模型提供商配置正确
- 检查容器中的 PowerMem 日志：`docker-compose logs voice-assistant | grep -i powermem`

#### 6. 端口已被占用

**问题**：端口 3000、8080 或 2881 已被使用的错误

**解决方案**：
```bash
# 查找使用端口的进程
lsof -i :3000
lsof -i :8080
lsof -i :2881

# 终止进程或在 docker-compose.yml 中更改端口
```

### 获取帮助

- 检查日志：`docker-compose logs -f`
- 查看 [PowerMem 文档](https://github.com/oceanbase/powermem/)
- 查看 [TEN Framework 文档](../../../AGENTS.md)
- 验证您的 `.env` 配置是否与上述示例匹配

### 重置所有内容

如果您需要重新开始：

```bash
# 停止并删除容器
docker-compose down -v

# 删除任何本地数据
rm -rf ./data

# 重新启动
docker-compose up -d
```
