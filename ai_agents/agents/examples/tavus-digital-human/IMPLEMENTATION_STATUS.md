# Tavus Digital Human Integration - Implementation Status

## 概述
将 Tavus Conversational Video Interface (CVI) 集成到 TEN framework 作为一个示例应用。

## 目标
1. 创建一个可重用的 Tavus extension
2. 提供一个简单的前端界面与数字人对话
3. 确保能在 Docker 容器中运行

## 已完成的工作

### 1. Extension 创建
**位置**: `ten_packages/extension/tavus_conversation_manager_python/`

#### 文件列表:
- ✅ `manifest.json` - Extension 元数据和 API 定义
- ✅ `property.json` - 默认属性配置
- ✅ `addon.py` - Extension 注册
- ✅ `extension.py` - 主要业务逻辑
- ✅ `requirements.txt` - Python 依赖 (httpx>=0.27.0)

#### 关键修复:
- **manifest.json 验证错误**: 修复了属性定义结构，使用正确的 `"properties"` 包裹
- **版本号错误**: 将 `ten_runtime_python` 从 `0.5.3` 修改为 `0.11`

### 2. Example 应用创建
**位置**: `examples/tavus-digital-human/`

#### 目录结构:
```
tavus-digital-human/
├── Dockerfile
├── Taskfile.docker.yml
├── Taskfile.yml
├── README.md
└── tenapp/
    ├── manifest.json
    ├── property.json
    ├── main.go
    ├── go.mod
    ├── go.sum
    ├── .tenignore
    └── scripts/
        └── start.sh
```

#### 已创建文件:
- ✅ `Dockerfile` - 生产环境 Docker 构建
- ✅ `Taskfile.docker.yml` - 容器内运行任务
- ✅ `Taskfile.yml` - 本地开发任务
- ✅ `README.md` - 使用文档
- ✅ `tenapp/manifest.json` - App 依赖和脚本定义
- ✅ `tenapp/property.json` - Graph 配置
- ✅ `tenapp/main.go` - Go 应用入口
- ✅ `tenapp/go.mod` - Go 模块定义
- ✅ `tenapp/go.sum` - Go 依赖校验
- ✅ `tenapp/scripts/start.sh` - 启动脚本
- ✅ `tenapp/.tenignore` - TEN 忽略文件

#### 关键修复:
- **go.mod 路径错误**: 修复为 `./ten_packages/system/ten_runtime_go/interface`
- **manifest.json scripts**: 修复为 `"start": "scripts/start.sh"` 而不是直接 `"bin/main"`

### 3. API Server 集成
**位置**: `server/internal/`

#### 修改的文件:
- ✅ `http_server.go` - 添加了 `/api/tavus/conversation/create` 端点
- ✅ `code.go` - 添加了错误码 `codeErrCallTavusApiFailed`

#### API 端点功能:
- 接收 persona_id (可选)
- 从环境变量读取 TAVUS_API_KEY
- 调用 Tavus API 创建 conversation
- 返回 conversation_url 给前端

### 4. Frontend 页面
**位置**: `playground/src/app/tavus/page.tsx`

#### 功能:
- 使用 @daily-co/daily-js SDK
- 调用 TEN API 创建 conversation
- 在页面中显示 Daily.co WebRTC 视频流
- 提供 Start/End Conversation 按钮

#### 依赖:
- ✅ 已添加 `@daily-co/daily-js@0.85.0` 到 `playground/package.json`

## 当前问题

### 问题 1: Go 二进制编译
**状态**: 🔴 阻塞

**问题描述**:
- `tman install` 没有自动构建 `tenapp/bin/main` 二进制文件
- 手动使用 `go build` 需要正确的 CGO 配置

**手动构建命令 (成功)**:
```bash
cd /app/agents/examples/tavus-digital-human/tenapp
export CGO_ENABLED=1
export CGO_LDFLAGS='-L./ten_packages/system/ten_runtime_go/lib -lten_runtime_go'
export CGO_CFLAGS='-I./ten_packages/system/ten_runtime_go/interface/ten_runtime'
go build -o bin/main -v .
```

**为什么 tman install 没有构建?**
对比 voice-assistant:
- voice-assistant 运行 `tman install` 时会看到 "Build GO app with command: go build -o bin/main -v ."
- tavus-digital-human 运行 `tman install` 时没有这个输出
- 可能原因: manifest.json 配置差异或 tman 检测逻辑问题

### 问题 2: 运行时错误
**状态**: 🔴 未测试

**错误信息**:
```
[agora_ok2bri] :-(  Error: Script 'start' exited with non-zero code: Some(127)
Worker process failed err="exit status 1"
```

**可能原因**:
1. `bin/main` 不存在 (已通过手动编译解决)
2. `scripts/start.sh` 缺少执行权限 (已设置)
3. 运行时依赖缺失
4. Python extension 初始化失败

## 需要进一步调试的步骤

### Step 1: 自动构建问题
需要理解为什么 `tman install` 不自动构建 Go 二进制。

**调查方向**:
1. 检查 `tman` 源码中的构建触发条件
2. 对比 voice-assistant 和 tavus-digital-human 的 manifest.json 差异
3. 检查是否需要特定的 manifest.json 字段来触发构建

**临时解决方案**:
在 `Taskfile.yml` 中添加显式构建步骤:

```yaml
build-tenapp:
  desc: build tenapp go binary
  dir: ./tenapp
  cmds:
    - |
      export CGO_ENABLED=1
      export CGO_LDFLAGS='-L./ten_packages/system/ten_runtime_go/lib -lten_runtime_go'
      export CGO_CFLAGS='-I./ten_packages/system/ten_runtime_go/interface/ten_runtime'
      go build -o bin/main -v .

install:
  desc: install dependencies
  dir: ./tenapp
  cmds:
    - task: install-tenapp
    - task: build-tenapp  # 添加这个
    - task: install-tenapp-python-deps
    - task: install-frontend
    - task: build-api-server
```

### Step 2: 测试运行
在容器中:
```bash
cd /app/agents/examples/tavus-digital-human
task install  # 重新安装以触发新的构建步骤
task run      # 启动服务
```

### Step 3: 检查日志
查看详细的运行日志:
```bash
# 在容器中
tail -f /tmp/ten_agent/app-*.log
```

### Step 4: 验证 Extension 加载
检查 Python extension 是否正确加载:
```bash
# 在容器中
cd /app/agents/examples/tavus-digital-human/tenapp
python3 -c "
import sys
sys.path.insert(0, './ten_packages/extension/tavus_conversation_manager_python')
from extension import TavusConversationManagerExtension
print('Extension import successful')
"
```

### Step 5: 测试前端
访问 http://localhost:3000/tavus 并:
1. 打开浏览器开发者工具查看 Console 错误
2. 点击 "Start Conversation" 按钮
3. 检查 Network 面板中的 API 请求/响应
4. 验证 Daily.co iframe 是否加载

## 架构说明

### 数据流:
```
Frontend (tavus/page.tsx)
    ↓ HTTP POST /api/tavus/conversation/create
API Server (http_server.go)
    ↓ HTTP POST https://tavusapi.com/v2/conversations
Tavus API
    ↓ 返回 conversation_url
Frontend
    ↓ Daily.join(conversation_url)
Daily.co WebRTC
    ↓ 视频流
User
```

### TEN Graph 配置:
目前 `property.json` 中定义了一个名为 `tavus_digital_human` 的 graph，包含一个 `tavus_manager` extension 节点。

**注意**: 当前实现中，Tavus extension 实际上没有在 TEN graph 中被使用。API server 直接调用 Tavus API。Extension 是为了未来扩展性而创建的（例如，将来可以在 TEN graph 中处理 Tavus 事件）。

## 依赖关系

### Python 依赖:
- `httpx>=0.27.0` (用于 Tavus API 调用)

### Go 依赖:
- `ten_framework/ten_runtime` (通过 go.mod replace)

### System 依赖:
- `ten_runtime_go@0.11`
- `ten_runtime_python@0.11`

### NPM 依赖:
- `@daily-co/daily-js@0.85.0`

## 环境变量

必需:
- `TAVUS_API_KEY` - Tavus API 密钥

可选:
- `TAVUS_PERSONA_ID` - 默认的 Persona ID

## 与其他 Examples 的差异

### 相似点:
- 使用相同的目录结构
- 使用 Taskfile.yml 管理任务
- 包含 Docker 支持
- 有 tenapp/main.go 入口

### 差异点:
1. **没有使用 Agora RTC**: 其他例子使用 Agora 进行音视频传输，Tavus 使用 Daily.co
2. **简化的 Graph**: 只有一个 extension 节点，没有 STT/LLM/TTS 管道
3. **Frontend 直接集成**: 使用 Daily.js SDK 而不是通过 Agora SDK

## 下一步行动

### 立即需要:
1. 🔴 **修复 Taskfile.yml 添加显式构建步骤** - 高优先级
2. 🔴 **测试完整运行流程** - 高优先级
3. 🟡 **调试任何运行时错误** - 中优先级

### 未来改进:
1. 🟢 **将 Tavus API 调用移到 Extension 中** - 低优先级
2. 🟢 **添加错误处理和重试逻辑** - 低优先级
3. 🟢 **支持 Tavus Echo Mode** - 低优先级
4. 🟢 **添加 Tavus 事件监听** - 低优先级

## 已知限制

1. **不支持 TEN Graph 控制**: 当前 Tavus conversation 完全由前端和 Tavus API 管理，不通过 TEN graph
2. **没有 STT/LLM 集成**: 使用 Tavus 内置的语音识别和语言模型
3. **需要 Daily.co 账号**: Tavus 使用 Daily.co 作为 WebRTC 基础设施
4. **仅支持创建 conversation**: 还没有实现 end_conversation 的完整逻辑

## 参考资料

- [Tavus API 文档](https://docs.tavus.io/sections/conversational-video-interface/quickstart/use-the-full-pipeline)
- [Daily.js SDK](https://docs.daily.co/reference/daily-js)
- [TEN Framework 文档](https://doc.theten.ai)

## 联系信息

如有问题，请参考:
- `examples/voice-assistant` - 最完整的参考实现
- `examples/demo` - 另一个简单的示例

## 更新日志

### 2025-11-26
- 创建基础 extension 和 example 结构
- 修复 manifest.json 验证错误
- 修复 go.mod 路径错误
- 添加手动构建步骤
- 文档化当前状态和待办事项
