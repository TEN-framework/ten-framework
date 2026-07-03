# DingTalk Bot Extension

一个用于 TEN Framework 的钉钉机器人扩展,允许 AI 代理通过钉钉群机器人发送消息通知。

## ✨ 功能特性

- 🤖 **LLM 工具集成**: 作为 LLM 可调用的工具,支持智能消息发送
- 📨 **消息推送**: 向钉钉群聊发送文本消息
- 🔐 **安全认证**: 支持钉钉机器人的 access_token 和签名验证
- ⚡ **异步处理**: 基于异步架构,高性能非阻塞操作
- 📝 **详细日志**: 完整的日志记录,便于调试和监控

## 🔧 系统要求

- Python 3.8 或更高版本
- TEN Runtime Python >= 0.11
- TEN AI Base >= 0.7
- 有效的钉钉群机器人 webhook

## 📦 安装依赖

```bash
pip install -r requirements.txt
```

依赖包括:

- `requests`: 用于发送 HTTP 请求到钉钉 API

## ⚙️ 配置

### 获取钉钉机器人凭证

1. 在钉钉群中添加自定义机器人
2. 选择"自定义"机器人类型
3. 设置安全设置(建议同时启用关键词和加签)
4. 获取 Webhook 地址中的 `access_token`
5. 如果启用了加签,获取 `secret` 密钥

### 配置文件设置

编辑 `property.json` 文件:

```json
{
    "access_token": "your_dingtalk_access_token_here",
    "secret": "your_dingtalk_secret_here"
}
```

**重要提示**:

- ⚠️ 不要将包含真实凭证的 `property.json` 提交到版本控制系统
- 建议使用环境变量或密钥管理服务来存储敏感信息

### 环境变量(可选)

也可以通过环境变量配置:

```bash
export DINGTALK_ACCESS_TOKEN="your_access_token"
export DINGTALK_SECRET="your_secret"
```

## 🚀 在 Voice Assistant 示例中集成钉钉扩展

以下是在 `voice-assistant` 示例中集成钉钉机器人扩展的完整步骤:

### 步骤 1: 添加扩展依赖

编辑 `examples/voice-assistant/tenapp/manifest.json`,在 `dependencies` 数组中添加:

```json
{
  "dependencies": [
    // ... 其他依赖 ...
    {
      "path": "../../../ten_packages/extension/dingtalk_bot_tool_python"
    }
  ]
}
```

**参考位置**: 在文件第 157 行附近,其他扩展依赖之后

### 步骤 2: 添加扩展节点

编辑 `examples/voice-assistant/tenapp/property.json`,在 `nodes` 数组中添加钉钉扩展节点:

```json
{
  "type": "extension",
  "name": "dingtalk_bot_tool_python",
  "addon": "dingtalk_bot_tool_python",
  "extension_group": "default",
  "property": {
    "access_token": "your_dingtalk_access_token_here",
    "secret": "your_dingtalk_secret_here"
  }
}
```

**参考位置**: 在 `property.json` 文件第 121-129 行,添加在其他工具扩展之后

### 步骤 3: 注册工具到主控制器

在 `property.json` 的 `connections` 部分,找到 `main_control` 的配置,添加钉钉扩展的工具注册连接:

```json
{
  "extension": "main_control",
  "cmd": [
    {
      "names": [
        "tool_register"
      ],
      "source": [
        {
          "extension": "weatherapi_tool_python"
        },
        {
          "extension": "dingtalk_bot_tool_python"
        }
      ]
    }
  ]
}
```

**参考位置**: 在 `property.json` 文件第 147-158 行

### 完整配置示例

以下是 `property.json` 中需要修改的关键部分:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant",
        "auto_start": true,
        "graph": {
          "nodes": [
            // ... 其他节点(agora_rtc, stt, llm, tts 等) ...
            
            // 添加钉钉扩展节点
            {
              "type": "extension",
              "name": "dingtalk_bot_tool_python",
              "addon": "dingtalk_bot_tool_python",
              "extension_group": "default",
              "property": {
                "access_token": "your_dingtalk_access_token_here",
                "secret": "your_dingtalk_secret_here"
              }
            }
          ],
          "connections": [
            {
              "extension": "main_control",
              "cmd": [
                // 注册工具到主控制器
                {
                  "names": ["tool_register"],
                  "source": [
                    {"extension": "weatherapi_tool_python"},
                    {"extension": "dingtalk_bot_tool_python"}  // 添加这一行
                  ]
                }
              ]
            }
            // ... 其他连接配置 ...
          ]
        }
      }
    ]
  }
}
```

### 配置说明

#### 必须修改的文件:

1. **`manifest.json`** (第 157 行)
   - 添加扩展路径依赖

2. **`property.json`** (两处修改)
   - **第 121-129 行**: 添加钉钉扩展节点配置
   - **第 155 行**: 在工具注册连接中添加钉钉扩展

#### 配置参数:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `access_token` | string | ✅ | 钉钉机器人的 access token |
| `secret` | string | ✅ | 钉钉机器人的加签密钥 |
| `extension_group` | string | ✅ | 扩展组,设置为 "default" |

### 使用效果

配置完成后,当用户对 AI 助手说话时,如果触发了需要发送钉钉通知的场景,AI 会自动调用钉钉扩展发送消息。

**示例对话:**

```
用户: "帮我通知团队今天下午3点开会"
AI: "好的,我已经向钉钉群发送了会议通知"
```

**钉钉群消息:**

```
今天下午3点开会
```

### 验证配置

1. 启动 voice-assistant 应用
2. 查看日志,确认钉钉扩展已加载:
   ```
   [DingTalkBotExtension] on_start BEGIN
   [DingTalkBotExtension] Config loaded successfully
   [DingTalkBotExtension] Tool registration result: ...
   ```
3. 与 AI 对话,测试钉钉消息发送功能

### 常见问题

**Q: 配置后扩展没有加载?**

- 检查 `manifest.json` 中的路径是否正确
- 确认已运行 `task install` 安装依赖

**Q: 消息发送失败?**

- 检查 `access_token` 和 `secret` 是否正确
- 查看日志中的错误码和错误信息
- 确认钉钉机器人的安全设置

**Q: 工具未注册到 LLM?**

- 检查 `connections` 中是否正确添加了钉钉扩展
- 确认 `tool_register` 命令的连接配置正确