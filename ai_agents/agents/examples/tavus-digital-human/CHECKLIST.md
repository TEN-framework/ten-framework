# Tavus Digital Human - 完整修改清单

## 📋 我做了什么

### 1. 创建了 Tavus Extension

#### 文件位置: `ten_packages/extension/tavus_conversation_manager_python/`

| 文件 | 状态 | 说明 |
|------|------|------|
| `manifest.json` | ✅ 已创建并修复 | 定义 extension API，修复了属性结构和版本号 |
| `property.json` | ✅ 已创建 | 默认配置，从环境变量读取 TAVUS_API_KEY |
| `addon.py` | ✅ 已创建 | 注册 extension |
| `extension.py` | ✅ 已创建 | 实现 create_conversation 和 end_conversation |
| `requirements.txt` | ✅ 已创建 | Python 依赖: httpx>=0.27.0 |
| `__init__.py` | ✅ 已创建 | Python 包初始化 |

**关键修复**:
- ❌ 最初: `"api": { "property": { "tavus_api_key": {...} } }`
- ✅ 修复为: `"api": { "property": { "properties": { "tavus_api_key": {...} } } }`
- ❌ 最初: `"version": "0.5.3"`
- ✅ 修复为: `"version": "0.11"`

### 2. 创建了 Example 应用

#### 文件位置: `examples/tavus-digital-human/`

| 文件 | 状态 | 说明 |
|------|------|------|
| `README.md` | ✅ 已创建 | 使用文档 |
| `Dockerfile` | ✅ 已创建 | 生产环境构建 |
| `Taskfile.yml` | ✅ 已创建并修复 | 开发任务定义，添加了 build-tenapp |
| `Taskfile.docker.yml` | ✅ 已创建 | 容器内运行任务 |
| `IMPLEMENTATION_STATUS.md` | ✅ 已创建 | 详细状态文档 |
| `DEBUG_GUIDE.md` | ✅ 已创建 | 调试指南 |
| `CHECKLIST.md` | ✅ 已创建 | 本文件 |

#### 文件位置: `examples/tavus-digital-human/tenapp/`

| 文件 | 状态 | 说明 |
|------|------|------|
| `manifest.json` | ✅ 已创建并修复 | App 依赖定义，修复了 scripts.start 路径 |
| `property.json` | ✅ 已创建 | Graph 配置，定义 tavus_manager 节点 |
| `main.go` | ✅ 已创建 | Go 应用入口（从 voice-assistant 复制） |
| `go.mod` | ✅ 已创建并修复 | Go 模块定义，修复了 ten_framework 路径 |
| `go.sum` | ✅ 已创建 | Go 依赖校验（空文件） |
| `.tenignore` | ✅ 已创建 | TEN 忽略规则 |
| `scripts/start.sh` | ✅ 已创建 | 启动脚本，设置环境变量并运行 bin/main |

**关键修复**:
- ❌ 最初 go.mod: `replace ten_framework/ten_runtime => ./ten_packages/system/ten_runtime_go`
- ✅ 修复为: `replace ten_framework => ./ten_packages/system/ten_runtime_go/interface`
- ❌ 最初 manifest.json: `"start": "bin/main"`
- ✅ 修复为: `"start": "scripts/start.sh"`

### 3. 修改了 API Server

#### 文件位置: `server/internal/`

| 文件 | 修改 | 说明 |
|------|------|------|
| `http_server.go` | ✅ 已添加 | 添加了 `/api/tavus/conversation/create` 端点 |
| `code.go` | ✅ 已添加 | 添加了错误码 `codeErrCallTavusApiFailed` |

**新增代码**:
```go
// http_server.go
func (s *HttpServer) handlerTavusCreateConversation(c *gin.Context) {
    // 调用 Tavus API 创建 conversation
    // 返回 conversation_url
}

// 路由注册
r.POST("/api/tavus/conversation/create", s.handlerTavusCreateConversation)
```

### 4. 创建了 Frontend 页面

#### 文件位置: `playground/src/app/tavus/`

| 文件 | 状态 | 说明 |
|------|------|------|
| `page.tsx` | ✅ 已创建 | Tavus 交互页面，使用 Daily.js SDK |

#### 文件位置: `playground/`

| 文件 | 修改 | 说明 |
|------|------|------|
| `package.json` | ✅ 已添加依赖 | 添加了 `@daily-co/daily-js@0.85.0` |

## 📝 还需要做什么

### 立即需要修复的问题

#### 1. 🔴 测试完整流程（高优先级）

在容器中运行:
```bash
cd /app/agents/examples/tavus-digital-human
rm -rf tenapp/bin tenapp/manifest-lock.json
task install
task run
```

**验证点**:
- [ ] `task install` 成功完成，没有错误
- [ ] `tenapp/bin/main` 文件存在
- [ ] `task run` 启动三个服务（API, Frontend, Graph Designer）
- [ ] 没有 "Script 'start' exited with non-zero code: Some(127)" 错误

#### 2. 🔴 验证前端功能（高优先级）

访问 http://localhost:3000/tavus

**验证点**:
- [ ] 页面加载成功
- [ ] 点击 "Start Conversation" 创建 conversation
- [ ] 看到 Daily.co 视频界面
- [ ] 可以与 Tavus 数字人交互

#### 3. 🟡 检查错误日志（中优先级）

如果运行失败，收集以下信息:
- [ ] `task run` 的完整输出
- [ ] `/tmp/ten_agent/app-*.log` 的内容
- [ ] 浏览器 Console 的错误信息
- [ ] 浏览器 Network 面板的 API 请求/响应

### 未来可以改进的地方

#### 1. 🟢 将 Tavus API 调用移到 Extension（低优先级）

**当前**: API Server 直接调用 Tavus API
**改进**: 通过 TEN graph 发送命令给 Extension，Extension 调用 Tavus API

**为什么要改**:
- 更符合 TEN 架构
- Extension 可以被其他应用重用
- 可以在 TEN graph 中监听和处理 Tavus 事件

**怎么改**:
1. 在 `http_server.go` 中，将 Tavus API 调用替换为发送 TEN command
2. 在 `extension.py` 的 `on_cmd` 中处理该命令
3. Extension 调用 Tavus API 后，发送 cmd_result 回 API server

#### 2. 🟢 添加错误处理（低优先级）

**需要处理的错误场景**:
- Tavus API 调用失败（401, 429, 500 等）
- Daily.co 连接失败
- Conversation 超时
- 网络断开重连

#### 3. 🟢 支持更多 Tavus 功能（低优先级）

**可以添加的功能**:
- Echo Mode（绕过 Tavus STT/LLM，使用 TEN 的）
- Conversation 状态监听
- 自定义 greeting 和 context
- 结束 conversation 的完整实现

#### 4. 🟢 优化 Docker 构建（低优先级）

**当前问题**: `tman install` 不自动构建 Go 二进制

**可能的解决方案**:
- 研究 `tman` 源码，找到触发构建的条件
- 或者在 Dockerfile 中显式添加构建步骤
- 或者创建自定义的 install 脚本

## 🔍 还需要调查的问题

### 问题 1: 为什么 tman install 不构建 bin/main?

**观察**:
- voice-assistant 运行 `tman install` 会看到 "Build GO app with command: go build -o bin/main -v ."
- tavus-digital-human 运行 `tman install` 没有这个输出

**可能原因**:
1. manifest.json 配置差异
2. tman 检测到某些文件/目录不存在
3. tman 版本或行为变化

**需要**:
- [ ] 对比 voice-assistant 和 tavus 的 manifest.json
- [ ] 检查 tman 日志或源码
- [ ] 尝试不同的 manifest.json 配置

### 问题 2: Extension 当前没有被实际使用

**观察**:
- Extension 已创建并在 manifest.json 中声明
- 但 API server 直接调用 Tavus API，没有通过 Extension

**影响**:
- Extension 代码实际上不会被执行
- TEN graph 中的 tavus_manager 节点是"空的"

**是否需要修复**:
- 不紧急，当前架构可以工作
- 但为了架构一致性，建议将 API 调用移到 Extension
- 或者删除 Extension，直接在文档中说明这是一个简化的实现

## ✅ 检查清单（用于验证）

### 文件存在性检查

在本地（macOS）:
```bash
cd /Users/chenyifan/Code/ten-framework/ai_agents

# Extension 文件
ls agents/ten_packages/extension/tavus_conversation_manager_python/manifest.json
ls agents/ten_packages/extension/tavus_conversation_manager_python/extension.py

# Example 文件
ls agents/examples/tavus-digital-human/README.md
ls agents/examples/tavus-digital-human/tenapp/main.go
ls agents/examples/tavus-digital-human/tenapp/scripts/start.sh

# API Server 修改
grep "handlerTavusCreateConversation" server/internal/http_server.go

# Frontend 文件
ls playground/src/app/tavus/page.tsx
grep "@daily-co/daily-js" playground/package.json
```

### 容器内检查

```bash
docker exec ten_agent_dev bash -c "
cd /app/agents/examples/tavus-digital-human &&
echo '=== Checking files ===' &&
ls -l tenapp/main.go tenapp/go.mod tenapp/scripts/start.sh &&
echo '=== Checking binary ===' &&
ls -l tenapp/bin/main 2>/dev/null || echo 'Binary not found - need to build' &&
echo '=== Checking extension ===' &&
ls -l ../../../ten_packages/extension/tavus_conversation_manager_python/extension.py
"
```

## 📊 当前状态总结

| 组件 | 状态 | 备注 |
|------|------|------|
| Extension 代码 | ✅ 完成 | 但实际未被使用 |
| Extension manifest | ✅ 完成并修复 | 修复了验证错误 |
| Example 目录结构 | ✅ 完成 | 所有文件已创建 |
| tenapp/main.go | ✅ 完成 | 从 voice-assistant 复制 |
| tenapp/go.mod | ✅ 完成并修复 | 修复了路径 |
| tenapp/scripts/start.sh | ✅ 完成 | 已设置可执行权限 |
| Taskfile.yml | ✅ 完成并修复 | 添加了 build-tenapp 任务 |
| API Server 集成 | ✅ 完成 | 添加了 Tavus 端点 |
| Frontend 页面 | ✅ 完成 | 使用 Daily.js SDK |
| 二进制构建 | ⚠️  手动可行 | tman install 不自动构建，但 Taskfile 已修复 |
| 运行测试 | ❓ 待验证 | 需要用户测试 |
| 端到端功能 | ❓ 待验证 | 需要用户测试 |

## 🚀 下一步行动

### 给用户的建议:

1. **重新安装和测试**:
   ```bash
   docker exec -it ten_agent_dev bash
   cd /app/agents/examples/tavus-digital-human
   rm -rf tenapp/bin tenapp/manifest-lock.json
   task install
   task run
   ```

2. **如果还是失败**:
   - 查看完整的错误日志
   - 对照 `DEBUG_GUIDE.md` 进行排查
   - 检查 `IMPLEMENTATION_STATUS.md` 了解架构

3. **如果成功**:
   - 访问 http://localhost:3000/tavus
   - 测试创建 conversation
   - 验证视频交互功能

4. **反馈信息**:
   - 如果工作了：太好了！可以考虑后续改进
   - 如果不工作：提供错误日志，我会继续调试

## 📞 需要的反馈

请告诉我:
1. ✅ or ❌: `task install` 是否成功?
2. ✅ or ❌: `tenapp/bin/main` 是否存在?
3. ✅ or ❌: `task run` 是否启动成功?
4. ✅ or ❌: 是否看到错误 "Script 'start' exited with non-zero code: Some(127)"?
5. ✅ or ❌: http://localhost:3000/tavus 是否可以访问?
6. ✅ or ❌: 是否能创建 conversation 并看到视频?

如果有任何步骤失败，请提供完整的错误信息。
