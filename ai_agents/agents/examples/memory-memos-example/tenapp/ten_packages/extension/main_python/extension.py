import asyncio
import json
import time
from typing import Literal

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    StatusCode,
)

from .agent.agent import Agent
from .agent.events import (
    ASRResultEvent,
    LLMResponseEvent,
    ToolRegisterEvent,
    UserJoinedEvent,
    UserLeftEvent,
)
from .helper import _send_cmd, _send_data, parse_sentences
from .config import MainControlConfig  # assume extracted from your base model

import uuid

# MemOS client
from .memory import MemosClient
from ten_ai_base.struct import LLMResponseRetrievePrompt


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module.
    Consumes semantic AgentEvents from the Agent class and drives the runtime behavior.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None
        self.agent: Agent = None
        self.config: MainControlConfig = None

        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"
        self.current_user_query: str = ""  # Track current user query for memory storage

        # MemOS client
        self.memos_client: MemosClient | None = None
        self.conversation_id: str = str(uuid.uuid4())

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        # Initialize MemOS client
        if self.config.enable_memorization:
            try:
                self.memos_client = MemosClient(
                    env=ten_env,
                    api_key=self.config.memos_api_key,
                    base_url=self.config.memos_base_url,
                )
            except Exception as e:
                ten_env.log_error(
                    f"[MainControlExtension] Failed to initialize MemOS client: {e}"
                )
                self.memos_client = None

        self.agent = Agent(ten_env)

        # Now auto-register decorated methods
        for attr_name in dir(self):
            fn = getattr(self, attr_name)
            event_type = getattr(fn, "_agent_event_type", None)
            if event_type:
                self.agent.on(event_type, fn)

    # === Register handlers with decorators ===
    @agent_event_handler(UserJoinedEvent)
    async def _on_user_joined(self, event: UserJoinedEvent):
        self._rtc_user_count += 1
        if self._rtc_user_count == 1 and self.config and self.config.greeting:
            await self._send_to_tts(self.config.greeting, True)
            await self._send_transcript(
                "assistant", self.config.greeting, True, 100
            )

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, event: UserLeftEvent):
        self._rtc_user_count -= 1

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
        self.session_id = event.metadata.get("session_id", "100")
        stream_id = int(self.session_id)
        if not event.text:
            return
        if event.final or len(event.text) > 2:
            await self._interrupt()
        if event.final:
            self.turn_id += 1
            # Store current user query for memory storage after LLM completion
            self.current_user_query = event.text

            # Get prompt with memory context and update LLM extension
            prompt = await self._get_prompt_with_memory(event.text)

            # Send user query normally (without memory embedded in message)
            await self.agent.queue_llm_input(event.text, prompt)
        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        if not event.is_final and event.type == "message":
            sentences, self.sentence_fragment = parse_sentences(
                self.sentence_fragment, event.delta
            )
            for s in sentences:
                await self._send_to_tts(s, False)

        if event.is_final and event.type == "message":
            remaining_text = self.sentence_fragment or ""
            self.sentence_fragment = ""
            await self._send_to_tts(remaining_text, True)

            # Memorize after every LLM completion (matching MemOS documentation pattern)
            if self.config.enable_memorization:
                await self._memorize_conversation(
                    user_query=self.current_user_query,
                    assistant_response=event.text
                )

        await self._send_transcript(
            "assistant",
            event.text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        await self.agent.on_data(data)

    # === helpers ===
    async def _send_transcript(
        self,
        role: str,
        text: str,
        final: bool,
        stream_id: int,
        data_type: Literal["text", "reasoning"] = "text",
    ):
        """
        Sends the transcript (ASR or LLM output) to the message collector.
        """
        if data_type == "text":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "transcribe",
                    "role": role,
                    "text": text,
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        elif data_type == "reasoning":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "raw",
                    "role": role,
                    "text": json.dumps(
                        {
                            "type": "reasoning",
                            "data": {
                                "text": text,
                            },
                        }
                    ),
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        if final:
            self.ten_env.log_info(
                f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
            )

    async def _send_to_tts(self, text: str, is_final: bool):
        """
        Sends a sentence to the TTS system.
        """
        request_id = f"tts-request-{self.turn_id}"
        await _send_data(
            self.ten_env,
            "tts_text_input",
            "tts",
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": is_final,
                "metadata": self._current_metadata(),
            },
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}"
        )

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        self.sentence_fragment = ""
        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")

    # === Memory related methods ===

    async def _retrieve_related_memory(self, query: str, user_id: str = None) -> str:
        """Retrieve related memory based on user query using MemOS search"""
        if not self.memos_client:
            return ""

        try:
            user_id = user_id or self.config.user_id

            self.ten_env.log_info(
                f"[MainControlExtension] Searching related memory with query: '{query}' (conversation_id={self.conversation_id})"
            )

            # Call MemOS search_memory API directly
            memories = await self.memos_client.search_memory(query, user_id, self.conversation_id)

            # Format memories as numbered list (matching MemOS documentation pattern)
            # Pattern from: https://memos-docs.openmem.net/cn/usecase/home_assistant
            if memories:
                formatted_memories = "## memory context:\n"
                for i, memory in enumerate(memories, 1):
                    formatted_memories += f"{i}. {memory}\n"
                memory_text = formatted_memories
            else:
                memory_text = ""

            self.ten_env.log_info(
                f"[MainControlExtension] Retrieved memory context: {memory_text}"
            )

            return memory_text
        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to retrieve memory context: {e}"
            )
            return ""

    async def _get_prompt_with_memory(self, query: str) -> str:
        """
        Retrieve original prompt from LLM extension, search for related memory,
        combine them, and send update_prompt command.

        Returns the combined prompt string.
        """
        # Generate request_id for retrieve_prompt and update_prompt commands
        request_id = str(uuid.uuid4())

        # Step 1: Retrieve original prompt from LLM extension
        cmd_result, _ = await _send_cmd(
            self.ten_env,
            "retrieve_prompt",
            "llm",
            {"request_id": request_id}
        )

        result_json, _ = cmd_result.get_property_to_json(None)

        response = LLMResponseRetrievePrompt.model_validate_json(result_json)
        original_prompt = response.prompt or ""

        self.ten_env.log_info(
            f"[MainControlExtension] Retrieved original prompt (length: {len(original_prompt)})"
        )

        # Step 2: Search memory before LLM query (matching MemOS documentation pattern)
        related_memory = await self._retrieve_related_memory(query)

        # Step 3: Build combined prompt (original + memory context)
        if not related_memory:
            prompt = original_prompt
        else:
            prompt = f"{original_prompt}\n\n## Memory Context:\n{related_memory}\n\nUse this memory context to provide more personalized and relevant responses."


        return prompt

    async def _memorize_conversation(
        self, user_query: str, assistant_response: str, user_id: str = None
    ):
        """Memorize the current conversation turn via MemOS (matching documentation pattern)"""
        if not self.memos_client:
            return

        if not user_query or not assistant_response:
            return

        try:
            user_id = user_id or self.config.user_id

            # Store just the current turn (user query + assistant response) as per MemOS pattern
            conversation_for_memory = [
                {"role": "user", "content": user_query},
                {"role": "assistant", "content": assistant_response}
            ]

            # Use add_message with simplified parameters
            asyncio.create_task(
                self.memos_client.add_message(
                    conversation=conversation_for_memory,
                    user_id=user_id,
                    conversation_id=self.conversation_id,
                )
            )

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to memorize conversation: {e}"
            )
