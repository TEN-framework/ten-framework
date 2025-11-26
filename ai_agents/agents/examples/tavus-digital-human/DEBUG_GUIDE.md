# Tavus Digital Human - 调试指南

## 快速测试步骤

### 1. 在容器中重新安装和构建

```bash
# 进入容器
docker exec -it ten_agent_dev bash

# 进入项目目录
cd /app/agents/examples/tavus-digital-human

# 清理旧的构建
rm -rf tenapp/bin tenapp/manifest-lock.json

# 重新安装（现在包含显式构建步骤）
task install
```

**期望输出**:
- 看到 "Build GO app" 或 go build 成功信息
- `tenapp/bin/main` 文件存在且大小约 2.4M

**验证**:
```bash
ls -lh tenapp/bin/main
# 应该看到: -rwxr-xr-x 1 root root 2.4M ... tenapp/bin/main
```

### 2. 启动服务

```bash
cd /app/agents/examples/tavus-digital-human
task run
```

**期望输出**:
- API server 启动在 8080 端口
- Frontend 启动在 3000 端口
- Graph Designer 启动在 49483 端口

**不应该看到的错误**:
- ❌ `Error: Script 'start' exited with non-zero code: Some(127)`
- ❌ `bash: bin/main: No such file or directory`

### 3. 测试前端

打开浏览器访问: http://localhost:3000/tavus

**检查清单**:
- [ ] 页面加载成功
- [ ] 看到 "Tavus Digital Human" 标题
- [ ] 看到 "Start Conversation" 按钮
- [ ] 打开开发者工具，Console 没有错误

### 4. 测试创建 Conversation

点击 "Start Conversation" 按钮

**在浏览器 Network 面板中检查**:
- [ ] 看到 POST 请求到 `/api/tavus/conversation/create`
- [ ] 响应状态码 200
- [ ] 响应包含 `conversation_url` 字段

**在浏览器 Console 中检查**:
- [ ] 看到 "Conversation created:" 日志
- [ ] Daily iframe 开始加载
- [ ] 没有 WebRTC 或 Daily.co 错误

### 5. 测试视频显示

**检查清单**:
- [ ] 看到 Daily.co 视频界面
- [ ] 可以看到 Tavus 数字人视频
- [ ] 可以听到音频
- [ ] 可以说话并与数字人交互

## 常见问题排查

### 问题 1: bin/main 不存在

**症状**:
```
Error: Script 'start' exited with non-zero code: Some(127)
```

**解决方案**:
```bash
cd /app/agents/examples/tavus-digital-human/tenapp
export CGO_ENABLED=1
export CGO_LDFLAGS='-L./ten_packages/system/ten_runtime_go/lib -lten_runtime_go'
export CGO_CFLAGS='-I./ten_packages/system/ten_runtime_go/interface/ten_runtime'
mkdir -p bin
go build -o bin/main -v .
```

### 问题 2: TAVUS_API_KEY 未设置

**症状**:
```
call tavus api failed: missing API key
```

**解决方案**:
```bash
# 检查 .env 文件
cat /app/.env | grep TAVUS_API_KEY

# 如果没有，添加:
echo "TAVUS_API_KEY=your_key_here" >> /app/.env
```

### 问题 3: Python 依赖缺失

**症状**:
```
ModuleNotFoundError: No module named 'httpx'
```

**解决方案**:
```bash
cd /app/agents/examples/tavus-digital-human/tenapp
pip install httpx>=0.27.0
```

### 问题 4: Frontend 页面 404

**症状**:
浏览器访问 http://localhost:3000/tavus 显示 404

**解决方案**:
检查 playground 是否在运行:
```bash
# 在容器中
cd /app/playground
bun run dev
```

### 问题 5: Tavus API 调用失败

**症状**:
```
call tavus api failed: status 401
```

**可能原因**:
1. TAVUS_API_KEY 无效
2. TAVUS_API_KEY 过期
3. Tavus 账号余额不足

**调试**:
```bash
# 直接测试 Tavus API
curl -X POST https://tavusapi.com/v2/conversations \
  -H "x-api-key: $TAVUS_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "persona_id": "your_persona_id",
    "conversational_context": "Test"
  }'
```

## 查看日志

### TEN App 日志
```bash
# 在容器中
tail -f /tmp/ten_agent/app-*.log
```

### API Server 日志
API server 的日志直接输出到终端（在运行 `task run` 的窗口）

### Frontend 日志
- 浏览器开发者工具 Console 面板
- 终端中 bun run dev 的输出

## 手动测试 API 端点

### 测试创建 Conversation
```bash
curl -X POST http://localhost:8080/api/tavus/conversation/create \
  -H "Content-Type: application/json" \
  -d '{
    "persona_id": "your_persona_id"
  }'
```

**期望响应**:
```json
{
  "code": "0",
  "data": {
    "conversation_id": "...",
    "conversation_url": "https://tavus.daily.co/...",
    "status": "active"
  }
}
```

## 验证 Extension 加载

```bash
# 在容器中
cd /app/agents/examples/tavus-digital-human/tenapp

# 测试 Python import
python3 -c "
import sys
sys.path.insert(0, './ten_packages/extension/tavus_conversation_manager_python')
from extension import TavusConversationManagerExtension
print('✅ Extension import successful')
"
```

## 检查文件权限

```bash
# 在容器中
cd /app/agents/examples/tavus-digital-human/tenapp

# 检查 start.sh 可执行权限
ls -l scripts/start.sh
# 应该显示: -rwxr-xr-x

# 如果没有执行权限，添加:
chmod +x scripts/start.sh

# 检查 bin/main 可执行权限
ls -l bin/main
# 应该显示: -rwxr-xr-x

# 如果没有执行权限，添加:
chmod +x bin/main
```

## 完整的清理和重建

如果遇到各种奇怪的问题，尝试完全清理并重建:

```bash
# 在容器中
cd /app/agents/examples/tavus-digital-human

# 清理所有构建产物
rm -rf tenapp/bin
rm -rf tenapp/manifest-lock.json
rm -rf tenapp/ten_packages
rm -rf tenapp/.release

# 清理 API server
rm -rf ../../../server/bin

# 清理 frontend node_modules (可选，慢)
# rm -rf ../../../playground/node_modules

# 重新安装
task install

# 重新运行
task run
```

## 与 voice-assistant 对比测试

如果 tavus 不工作，可以先确认 voice-assistant 是否工作:

```bash
# 在容器中
cd /app/agents/examples/voice-assistant

task install
task run
```

访问 http://localhost:3000

如果 voice-assistant 工作但 tavus 不工作，说明是 tavus 特定的问题。
如果两者都不工作，说明是环境配置问题。

## 下一步调试

如果按照以上步骤仍然无法工作，请提供:

1. **完整的错误日志**:
   ```bash
   # 保存日志到文件
   cd /app/agents/examples/tavus-digital-human
   task run > /tmp/tavus-debug.log 2>&1
   ```

2. **TEN app 日志**:
   ```bash
   cat /tmp/ten_agent/app-*.log
   ```

3. **文件存在性检查**:
   ```bash
   ls -laR /app/agents/examples/tavus-digital-human/tenapp/ | grep -E '(main.go|bin/main|start.sh)'
   ```

4. **环境变量检查**:
   ```bash
   env | grep -E '(TAVUS|TEN|GO)'
   ```

5. **浏览器 Console 错误**:
   - F12 打开开发者工具
   - 截图或复制所有红色错误信息
