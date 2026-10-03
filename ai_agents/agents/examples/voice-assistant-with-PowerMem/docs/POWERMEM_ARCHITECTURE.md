# PowerMem 在 TEN Framework 中的架构文档

## 目录

1. [为什么语音代理需要记忆？](#为什么语音代理需要记忆)
2. [PowerMem 如何融入 TEN Framework 工作流程](#powermem-如何融入-ten-framework-工作流程)
3. [数据的存储、检索与更新](#数据的存储检索与更新)
4. [记忆如何改变语音代理的行为](#记忆如何改变语音代理的行为)
5. [代码详解](#代码详解)

---

## 为什么语音代理需要记忆？

### 无记忆的语音代理的局限性

传统的语音代理（Voice Agent）在每次对话中都是"失忆"的：

```
用户第1天：我叫张三，喜欢喝咖啡
AI：好的，很高兴认识你张三！

用户第2天：你还记得我喜欢喝什么吗？
AI：抱歉，我不知道你的偏好...  ❌ 无法记住之前的对话
```

### 有记忆的语音代理的优势

引入 PowerMem 后：

```
用户第1天：我叫张三，喜欢喝咖啡
AI：好的，很高兴认识你张三！[自动保存记忆]

用户第2天：你还记得我喜欢喝什么吗？
AI：当然记得！你喜欢喝咖啡 ☕  ✅ 从记忆中检索
```

### 记忆为语音代理带来的核心价值

| 能力 | 无记忆 | 有记忆 |
|------|--------|--------|
| 个性化问候 | 通用问候 | 基于用户历史的个性化问候 |
| 上下文连贯性 | 每次对话独立 | 跨会话保持上下文 |
| 用户偏好 | 每次重新询问 | 自动记住并应用 |
| 情感连接 | 机械式回复 | 更像"老朋友"的交互 |

---

## PowerMem 如何融入 TEN Framework 工作流程

### TEN Framework 图架构概览

TEN Framework 使用**图（Graph）架构**来组织语音代理的各个组件：

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          TEN Framework Graph                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   ┌─────────┐     ┌─────────┐     ┌────────────────┐     ┌─────────┐    │
│   │  RTC    │────▶│   STT   │────▶│  main_control  │────▶│   TTS   │    │
│   │ (Agora) │     │(Deepgram)│     │  (PowerMem)   │     │(ElevenLabs)   │
│   └─────────┘     └─────────┘     └────────┬───────┘     └─────────┘    │
│        │                                    │                  │         │
│        │                                    ▼                  │         │
│        │                              ┌─────────┐              │         │
│        │                              │   LLM   │              │         │
│        │                              │(OpenAI) │              │         │
│        │                              └─────────┘              │         │
│        │                                                       │         │
│        └───────────────────◀───────────────────────────────────┘         │
│                           Audio Output                                   │
└──────────────────────────────────────────────────────────────────────────┘
```

### PowerMem 在图中的位置

PowerMem 集成在 `main_control` 扩展中，作为对话的"大脑"：

```
                    ┌──────────────────────────────────────┐
                    │         main_control Extension       │
                    │  ┌────────────────────────────────┐  │
                    │  │         MemoryStore            │  │
                    │  │   ┌──────────────────────┐    │  │
User Speech ───────▶│  │   │     PowerMem SDK     │    │──▶ LLM with Context
                    │  │   │  ┌────────────────┐  │    │  │
                    │  │   │  │ Vector Store   │  │    │  │
                    │  │   │  │ (OceanBase)    │  │    │  │
                    │  │   │  └────────────────┘  │    │  │
                    │  │   └──────────────────────┘    │  │
                    │  └────────────────────────────────┘  │
                    └──────────────────────────────────────┘
```

### 完整数据流

```
1. 用户加入会话
   │
   ▼
2. 检索用户档案 (get_user_profile)
   │
   ▼
3. 生成个性化问候
   │
   ▼
4. 用户开始说话
   │
   ▼
5. STT 转写为文本 ──▶ 6. 语义搜索相关记忆 (search)
                              │
                              ▼
7. 构建带记忆上下文的提示词 ──▶ 8. LLM 生成回复
                                       │
                                       ▼
9. TTS 合成语音 ──▶ 10. 播放给用户
         │
         ▼
11. 定期/空闲时保存对话记忆 (add)
```

---

## 数据的存储、检索与更新

### 存储的数据类型

PowerMem 存储三种主要数据：

#### 1. 对话消息 (Conversation Messages)

```json
[
  {"role": "user", "content": "我叫张三，在北京工作"},
  {"role": "assistant", "content": "你好张三！北京是个很棒的城市..."},
  {"role": "user", "content": "我喜欢周末去爬山"},
  {"role": "assistant", "content": "爬山是很好的运动！你有最喜欢的山吗？"}
]
```

#### 2. 语义记忆 (Semantic Memories)

PowerMem 会自动从对话中提取关键信息：

```
- 用户名字：张三
- 工作地点：北京
- 兴趣爱好：爬山
- 语言偏好：中文
```

#### 3. 用户档案 (User Profile)

当启用 `enable_user_memory` 时，会维护用户档案：

```
User Profile:
- 用户经常在早上使用语音助手
- 用户对科技新闻感兴趣
- 用户偏好简短的回复
```

### 存储机制

```python
# memory.py 中的存储实现

async def add(
    self,
    conversation: list[dict],  # 对话历史
    user_id: str,              # 用户唯一标识
    agent_id: str,             # 代理唯一标识
) -> None:
    self.client.add(
        messages=conversation,
        user_id=user_id,
        agent_id=agent_id,
    )
```

**存储时机**：
- 基于轮次：每 N 轮对话保存一次（默认 5 轮）
- 空闲超时：用户 N 秒无活动时保存（默认 30 秒）
- 会话结束：用户退出时保存最终状态

### 检索机制

```python
# memory.py 中的检索实现

async def search(
    self, user_id: str, agent_id: str, query: str
) -> Any:
    result = self.client.search(
        user_id=user_id,
        agent_id=agent_id,
        query=query  # 用户的当前问题用于语义匹配
    )
    return result
```

**检索流程**：

```
用户查询: "我之前说过喜欢什么运动？"
         │
         ▼
    语义向量化
         │
         ▼
    在 OceanBase 中进行相似度搜索
         │
         ▼
    返回相关记忆:
    - "用户喜欢周末去爬山"
    - "用户说爬山是很好的运动"
```

### 更新机制

数据更新通过 **增量添加** 实现：

```
Time T1: 对话 1-5 ──▶ 保存 ──▶ PowerMem 提取关键信息
         │
Time T2: 对话 6-10 ──▶ 保存 ──▶ PowerMem 更新/补充信息
         │
Time T3: 对话 11-15 ──▶ 保存 ──▶ PowerMem 进一步完善用户画像
```

---

## 记忆如何改变语音代理的行为

### 行为变化对比

#### 场景 1：用户第一次加入

**无记忆**：
```
AI: Hello! I'm your AI assistant. How can I help you today?
```

**有记忆（新用户）**：
```
AI: Hello! I'm your AI assistant with memory. I can remember our
    previous conversations to provide more personalized help.
```

**有记忆（回访用户）**：
```
AI: 张三你好！好久不见！上次你说在准备爬香山，
    那次爬山怎么样？
```

#### 场景 2：用户查询

**无记忆**：
```
用户: 帮我推荐一个周末活动
AI: 您可以考虑去看电影、逛公园、或者参观博物馆...（通用建议）
```

**有记忆**：
```
用户: 帮我推荐一个周末活动
AI: 我记得你喜欢爬山！这个周末天气不错，
    北京周边的妙峰山或者百花山都是很好的选择。（个性化建议）
```

### 记忆影响行为的三个阶段

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          用户加入时                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 获取用户档案 ──▶ 2. 生成个性化问候 ──▶ 3. 通过 TTS 播放               │
│                                                                         │
│  [代码实现]                                                              │
│  memory_summary = await memory_store.get_user_profile(user_id, agent_id)│
│  greeting = await llm.generate(PERSONALIZED_GREETING_TEMPLATE,          │
│                                memory_summary)                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                          对话过程中                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 用户提问 ──▶ 2. 检索相关记忆 ──▶ 3. 注入 LLM 上下文 ──▶ 4. 回复       │
│                                                                         │
│  [代码实现]                                                              │
│  related_memory = await memory_store.search(user_id, agent_id, query)   │
│  context = CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE.format(                 │
│      related_memory=related_memory,                                     │
│      user_query=query                                                   │
│  )                                                                      │
│  await agent.queue_llm_input(context)                                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                          对话结束后                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 提取对话 ──▶ 2. 筛选用户/助手消息 ──▶ 3. 保存到 PowerMem              │
│                                                                         │
│  [代码实现]                                                              │
│  conversation = []                                                      │
│  for msg in llm_context:                                                │
│      if msg.role in ["user", "assistant"]:                             │
│          conversation.append({"role": msg.role, "content": msg.content})│
│  await memory_store.add(conversation, user_id, agent_id)               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 随时间演变的行为

```
     会话 1              会话 2              会话 3              会话 N
        │                   │                   │                   │
        ▼                   ▼                   ▼                   ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ 记忆: []     │    │ 记忆: [...]  │    │ 记忆: [...] │    │ 记忆: [...]  │
│ 行为: 通用   │──▶│ 行为: 初步   │──▶│ 行为: 较好  │──▶│ 行为: 精准   │
│ 问候: 默认   │    │ 问候: 带名字 │    │ 问候: 带偏好│    │ 问候: 完全   │
└──────────────┘    └──────────────┘    └──────────────┘    │       个性化 │
                                                            └──────────────┘

记忆积累: ═══════════════════════════════════════════════════════════════▶
个性化程度: ════════════════════════════════════════════════════════════▶
```

---

## 代码详解

### 核心类结构

```
┌─────────────────────────────────────────────────────────────────────┐
│                      MemoryStore (Abstract)                         │
│  ├── add(conversation, user_id, agent_id)      # 保存对话          │
│  ├── search(user_id, agent_id, query)          # 语义搜索          │
│  └── get_user_profile(user_id, agent_id)       # 获取用户档案      │
└──────────────────────────────────┬──────────────────────────────────┘
                                   │
           ┌───────────────────────┼───────────────────────┐
           │                       │                       │
           ▼                       ▼                       ▼
┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐
│PowerMemSdkMemoryStore│ │PowerMemSdkUserMemoryStore│  (可扩展其他实现)
│                    │  │                    │  │                    │
│ client: Memory     │  │ client: UserMemory │  │                    │
│                    │  │                    │  │                    │
│ 使用场景:          │  │ 使用场景:          │  │                    │
│ enable_user_memory │  │ enable_user_memory │  │                    │
│ = false            │  │ = true             │  │                    │
└────────────────────┘  └────────────────────┘  └────────────────────┘
```

### 配置选项详解

```python
# config.py

class MainControlConfig(BaseModel):
    # 问候语（会被个性化问候替换）
    greeting: str = "Hello! I'm your AI assistant with memory..."

    # 身份标识
    agent_id: str = "voice_assistant_agent"  # 代理ID（用于隔离不同代理的记忆）
    user_id: str = "user"                     # 用户ID（用于隔离不同用户的记忆）

    # 功能开关
    enable_memorization: bool = True   # 是否启用记忆功能
    enable_user_memory: bool = True    # 是否使用增强的用户档案功能

    # 保存策略
    memory_save_interval_turns: int = 5   # 每隔多少轮保存
    memory_idle_timeout_seconds: float = 30.0  # 空闲多少秒后保存

    # PowerMem 配置
    powermem_config: Dict[str, Any] = Field(default_factory=dict)
```

### 关键方法解析

#### 1. 生成个性化问候

```python
# extension.py

async def _generate_personalized_greeting(self) -> str:
    """
    基于用户记忆生成个性化问候

    流程:
    1. 检查记忆是否启用
    2. 获取用户档案/历史记忆
    3. 使用 LLM 生成问候
    4. 超时则返回空（使用默认问候）
    """
    if not self.memory_store or not self.config.enable_memorization:
        return ""

    try:
        # 获取用户档案
        memory_summary = await self.memory_store.get_user_profile(
            user_id=self.config.user_id,
            agent_id=self.config.agent_id,
        )

        if not memory_summary or not memory_summary.strip():
            return ""  # 无记忆，使用默认问候

        # 构建问候提示词
        greeting_prompt = PERSONALIZED_GREETING_TEMPLATE.format(
            memory_summary=memory_summary
        )

        # 异步等待 LLM 生成问候（10秒超时）
        self._greeting_future = asyncio.Future()
        self._is_generating_greeting = True
        await self.agent.queue_llm_input(greeting_prompt)

        greeting = await asyncio.wait_for(self._greeting_future, timeout=10.0)
        return greeting

    except asyncio.TimeoutError:
        return ""  # 超时，使用默认问候
```

#### 2. 检索相关记忆

```python
# extension.py

async def _retrieve_related_memory(self, query: str) -> str:
    """
    基于用户查询检索相关记忆

    参数:
        query: 用户的当前问题

    返回:
        格式化的记忆文本，用于注入 LLM 上下文
    """
    if not self.memory_store:
        return ""

    try:
        # 调用语义搜索 API
        resp = await self.memory_store.search(
            user_id=self.config.user_id,
            agent_id=self.config.agent_id,
            query=query
        )

        if not resp or not isinstance(resp, dict):
            return ""

        # 提取记忆内容
        results = resp.get("results", [])
        memories = [
            result["memory"]
            for result in results
            if isinstance(result, dict) and result.get("memory")
        ]

        # 格式化输出
        if memories:
            return "Memorise:\n" + "\n".join(f"- {m}" for m in memories) + "\n"
        return ""

    except Exception as e:
        self.ten_env.log_error(f"Failed to retrieve memory: {e}")
        return ""
```

#### 3. 保存对话记忆

```python
# extension.py

async def _memorize_conversation(self):
    """
    保存当前对话到 PowerMem

    保存策略:
    1. 从 LLM 上下文读取对话
    2. 过滤只保留 user/assistant 消息（排除 system）
    3. 调用 PowerMem SDK 保存
    """
    if not self.memory_store:
        return

    try:
        # 读取 LLM 上下文
        llm_context = (
            self.agent.llm_exec.get_context()
            if self.agent and self.agent.llm_exec
            else []
        )

        # 筛选用户和助手的消息
        conversation_for_memory = []
        for msg in llm_context:
            role = getattr(msg, "role", None)
            content = getattr(msg, "content", None)
            if role in ["user", "assistant"] and isinstance(content, str):
                conversation_for_memory.append({
                    "role": role,
                    "content": content
                })

        if not conversation_for_memory:
            return

        # 保存到 PowerMem
        await self.memory_store.add(
            conversation=conversation_for_memory,
            user_id=self.config.user_id,
            agent_id=self.config.agent_id,
        )

        # 更新计数器
        if self.turn_id > self.last_memory_update_turn_id:
            self.last_memory_update_turn_id = self.turn_id

    except Exception as e:
        self.ten_env.log_error(f"Failed to memorize: {e}")
```

### 提示词模板

```python
# prompt.py

# 带记忆的上下文模板
CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE = """Here's what I remember from our past conversations that might be relevant:

{related_memory}

Now the user is asking: {user_query}

Please respond naturally, as if you're continuing our conversation. Reference the memories when relevant, but keep it conversational and helpful."""

# 个性化问候生成模板
PERSONALIZED_GREETING_TEMPLATE = """You are a friendly and helpful voice assistant. Based on the following memory summary of previous conversations with this user, generate a warm, personalized greeting (2-3 sentences maximum). Reference specific details from the memories naturally, but keep it concise and friendly.

If the memory summary contains information about the user's location/region, please respond in the most commonly used language of that region.

Memory Summary:
{memory_summary}

Generate a personalized greeting:"""
```

---

## 快速开始

### 1. 配置环境变量

```bash
# .env 文件

# 数据库配置
DATABASE_PROVIDER=oceanbase
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=oceanbase
OCEANBASE_COLLECTION=memories

# LLM 配置（用于 PowerMem 内部处理）
LLM_PROVIDER=qwen
LLM_API_KEY=your_qwen_api_key
LLM_MODEL=qwen-plus

# Embedding 配置
EMBEDDING_PROVIDER=qwen
EMBEDDING_API_KEY=your_qwen_api_key
EMBEDDING_MODEL=text-embedding-v4
EMBEDDING_DIMS=1536
```

### 2. 启动向量数据库

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

### 3. 运行语音助手

```bash
cd ai_agents/agents/examples/voice-assistant-with-PowerMem
task install
task run
```

---

## 总结

PowerMem 通过以下方式为语音代理赋予"记忆"能力：

1. **存储**：自动保存对话历史，提取关键信息
2. **检索**：基于语义相似度找到相关记忆
3. **应用**：将记忆注入 LLM 上下文，实现个性化响应

这使得语音代理能够：
- 记住用户的名字、偏好和历史
- 提供更自然、更个性化的交互体验
- 随着时间推移不断了解用户，变得越来越"懂你"
