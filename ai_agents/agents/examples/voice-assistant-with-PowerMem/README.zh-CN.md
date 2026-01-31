# 语音助手（集成 PowerMem）

基于 TEN Framework 的记忆型语音助手示例。PowerMem 提供持久的语义记忆，使助手能够记住用户偏好、总结过往上下文，并在多次会话之间保持一致的行为。

本文档聚焦“为什么要有记忆”和“记忆如何工作”，而非 API 参考细节。

## 为什么语音助手需要记忆？

语音对话是瞬时的。没有记忆，每次会话都从零开始，助手会反复询问基础信息（姓名、偏好、目标）。有了记忆，助手可以：

- 跨会话保持连续性（“上次你说更喜欢简短回答”）
- 不再重复提问，提供更个性化的体验
- 通过回忆上下文解决含糊追问
- 随时间积累偏好与事实，体验越来越好

## 架构概览

TEN 图负责把语音、LLM 和 TTS 串联起来。PowerMem 插在控制层，负责记忆的检索与保存。

```mermaid
flowchart LR
  mic((User Voice)) --> agora[agora_rtc]
  agora --> stream[streamid_adapter]
  stream --> stt[deepgram_asr_python]
  stt --> main[main_python (main_control)]
  main --> llm[openai_llm2_python]
  llm --> main
  main --> tts[elevenlabs_tts2_python]
  tts --> agora
  main --> collector[message_collector2]

  main -. "search / add" .-> powermem[PowerMem\n(UserMemory/Memory)]
```

关键点：`main_python` 是编排者，决定何时检索记忆、如何注入 LLM 上下文，以及何时保存新记忆。

## PowerMem 如何融入 TEN 工作流

PowerMem 通过 `main_control` 扩展接入：

- **用户加入**：`main_python` 从 PowerMem 获取用户画像，用于生成个性化欢迎语
- **ASR 最终结果**：对用户话语做语义检索，把相关记忆注入 LLM 提示词
- **LLM 最终响应**：按配置规则把新对话写入 PowerMem
- **空闲超时 / 关闭**：保存尚未落库的对话内容

上述逻辑由 `tenapp/property.json` 中的 `powermem_config` 与 `enable_memorization` 驱动。

## 存储、检索与更新的数据是什么？

写入（存储到 PowerMem）：
- `messages`：只保存 LLM 上下文中的 user/assistant 消息（系统消息会被过滤）
- `user_id` / `agent_id`：按用户与助手隔离记忆

读取（从 PowerMem 检索）：
- **相关记忆**：根据当前用户问题进行语义检索
- **用户画像**：用于欢迎语个性化（`enable_user_memory` 时来自 `UserMemory.profile()`）

更新（随时间变化）：
- 助手只追加新对话；索引、向量化与排序由 PowerMem 负责

## 单轮对话流程（Step-by-step）

1. **音频输入**：用户通过 Agora 说话
2. **ASR**：语音转文本生成最终转写
3. **记忆检索**：`main_python` 用转写内容查询 PowerMem
4. **提示词增强**：相关记忆与用户问题一起注入 LLM
5. **LLM 响应**：生成带记忆上下文的答案
6. **TTS 输出**：通过 Agora 播放语音
7. **记忆保存**：按轮次或空闲超时写入 PowerMem

## 记忆生命周期与保存规则

保存触发有两种，并会协同避免重复保存：

- **按轮次保存**：每 `memory_save_interval_turns` 轮（默认 5）
- **空闲保存**：空闲 `memory_idle_timeout_seconds` 秒后（默认 30 秒）

伪流程（简化）：

```text
on_final_llm_response:
  if turns_since_last_save >= interval:
    save_to_powermem()
    cancel_idle_timer()
  else:
    start_or_reset_idle_timer()

on_idle_timeout:
  if unsaved_turns:
    save_to_powermem()
```

应用关闭时，会保存最后未落库的对话。

## 记忆如何改变行为

跨多次会话后，助手可以：

- 个性化欢迎用户（“欢迎回来，你仍然喜欢简短回答吗？”）
- 根据历史任务推断意图（“你上周问过徒步路线，这里有新的推荐”）
- 记住用户反馈，保持一致的语气和工具使用习惯

这是因为相关记忆在每次响应前被注入到 LLM 上下文中。

## 注释配置（main_control）

```json
{
  "name": "main_control",
  "addon": "main_python",
  "property": {
    "agent_id": "voice_assistant_agent",
    "user_id": "user",
    "enable_memorization": true,
    "enable_user_memory": true,
    "memory_save_interval_turns": 5,
    "memory_idle_timeout_seconds": 30.0,
    "powermem_config": {
      "vector_store": { "...": "OceanBase/SeekDB settings" },
      "llm": { "...": "LLM for memory processing" },
      "embedder": { "...": "Embedding model for search" }
    }
  }
}
```

提示：为每位真实用户设置不同的 `user_id`，为不同助手人格设置不同的 `agent_id`，可实现隔离记忆。

## 关键文件索引

- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/property.json` - 图配置与 PowerMem 入口
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/extension.py` - 记忆检索与保存逻辑
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/memory.py` - PowerMem 存储适配
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/prompt.py` - 记忆注入模板

## PowerMem 配置

在 `.env` 中设置环境变量：

```bash
# Database
DATABASE_PROVIDER=oceanbase
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=oceanbase
OCEANBASE_COLLECTION=memories

# LLM Provider (for PowerMem)
LLM_PROVIDER=qwen
LLM_API_KEY=your_qwen_api_key
LLM_MODEL=qwen-plus

# Embedding Provider (for PowerMem)
EMBEDDING_PROVIDER=qwen
EMBEDDING_API_KEY=your_qwen_api_key
EMBEDDING_MODEL=text-embedding-v4
EMBEDDING_DIMS=1536
```

## 快速开始

1. **启动 SeekDB 服务**
   ```bash
   docker run -d \
      --name seekdb \
      -p 2881:2881 \
      -p 2886:2886 \
      -v ./data:/var/lib/oceanbase \
      -e SEEKDB_DATABASE=powermem \
      -e ROOT_PASSWORD=password \
      oceanbase/seekdb:latest
   ```

2. **安装依赖**
   ```bash
   task install
   ```

3. **运行集成 PowerMem 的语音助手**
   ```bash
   task run
   ```

4. **访问应用**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483
