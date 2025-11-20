# DingTalk Bot Extension

A DingTalk bot extension for TEN Framework that enables AI agents to send message notifications through DingTalk group bots.

## ✨ Features

- 🤖 **LLM Tool Integration**: Works as an LLM-callable tool, supporting intelligent message sending
- 📨 **Message Push**: Send text messages to DingTalk group chats
- 🔐 **Security Authentication**: Supports DingTalk bot's access_token and signature verification
- ⚡ **Async Processing**: Built on async architecture with high-performance non-blocking operations
- 📝 **Detailed Logging**: Complete logging for easy debugging and monitoring

## 🔧 System Requirements

- Python 3.8 or higher
- TEN Runtime Python >= 0.11
- TEN AI Base >= 0.7
- Valid DingTalk group bot webhook

## 📦 Install Dependencies

```bash
pip install -r requirements.txt
```

Dependencies include:

- `requests`: For sending HTTP requests to DingTalk API

## ⚙️ Configuration

### Getting DingTalk Bot Credentials

1. Add a custom bot to your DingTalk group
2. Select "Custom" bot type
3. Configure security settings (recommended to enable both keywords and signing)
4. Get the `access_token` from the Webhook URL
5. If signing is enabled, get the `secret` key

### Configuration File Setup

Edit the `property.json` file:

```json
{
    "access_token": "your_dingtalk_access_token_here",
    "secret": "your_dingtalk_secret_here"
}
```

**Important Notes**:

- ⚠️ Do not commit `property.json` with real credentials to version control
- Recommended to use environment variables or secret management services to store sensitive information

### Environment Variables (Optional)

You can also configure via environment variables:

```bash
export DINGTALK_ACCESS_TOKEN="your_access_token"
export DINGTALK_SECRET="your_secret"
```

## 🚀 Integrating DingTalk Extension in Voice Assistant Example

Here are the complete steps to integrate the DingTalk bot extension in the `voice-assistant` example:

### Step 1: Add Extension Dependency

Edit `examples/voice-assistant/tenapp/manifest.json` and add to the `dependencies` array:

```json
{
  "dependencies": [
    // ... other dependencies ...
    {
      "path": "../../../ten_packages/extension/dingtalk_bot_tool_python"
    }
  ]
}
```

**Reference Location**: Around line 157 in the file, after other extension dependencies

### Step 2: Add Extension Node

Edit `examples/voice-assistant/tenapp/property.json` and add the DingTalk extension node to the `nodes` array:

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

**Reference Location**: Lines 121-129 in the `property.json` file, add after other tool extensions

### Step 3: Register Tool to Main Controller

In the `connections` section of `property.json`, find the `main_control` configuration and add the DingTalk extension's tool registration connection:

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

**Reference Location**: Lines 147-158 in the `property.json` file

### Complete Configuration Example

Here are the key parts that need to be modified in `property.json`:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant",
        "auto_start": true,
        "graph": {
          "nodes": [
            // ... other nodes (agora_rtc, stt, llm, tts, etc.) ...
            
            // Add DingTalk extension node
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
                // Register tool to main controller
                {
                  "names": ["tool_register"],
                  "source": [
                    {"extension": "weatherapi_tool_python"},
                    {"extension": "dingtalk_bot_tool_python"}  // Add this line
                  ]
                }
              ]
            }
            // ... other connection configurations ...
          ]
        }
      }
    ]
  }
}
```

### Configuration Instructions

#### Required File Modifications:

1. **`manifest.json`** (Line 157)
   - Add extension path dependency

2. **`property.json`** (Two modifications)
   - **Lines 121-129**: Add DingTalk extension node configuration
   - **Line 155**: Add DingTalk extension to tool registration connections

#### Configuration Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `access_token` | string | ✅ | DingTalk bot's access token |
| `secret` | string | ✅ | DingTalk bot's signing secret key |
| `extension_group` | string | ✅ | Extension group, set to "default" |

### Usage Effect

After configuration, when users speak to the AI assistant, if a scenario requiring DingTalk notification is triggered, the AI will automatically call the DingTalk extension to send messages.

**Example Conversation:**

```
User: "Please notify the team about the meeting at 3 PM today"
AI: "Okay, I've sent the meeting notification to the DingTalk group"
```

**DingTalk Group Message:**

```
Meeting at 3 PM today
```

### Verify Configuration

1. Start the voice-assistant application
2. Check logs to confirm DingTalk extension is loaded:
   ```
   [DingTalkBotExtension] on_start BEGIN
   [DingTalkBotExtension] Config loaded successfully
   [DingTalkBotExtension] Tool registration result: ...
   ```
3. Talk to the AI to test DingTalk message sending functionality

### FAQ

**Q: Extension not loading after configuration?**

- Check if the path in `manifest.json` is correct
- Confirm that `task install` has been run to install dependencies

**Q: Message sending failed?**

- Check if `access_token` and `secret` are correct
- Review error codes and messages in logs
- Verify DingTalk bot's security settings

**Q: Tool not registered to LLM?**

- Check if DingTalk extension is correctly added in `connections`
- Confirm the `tool_register` command connection configuration is correct