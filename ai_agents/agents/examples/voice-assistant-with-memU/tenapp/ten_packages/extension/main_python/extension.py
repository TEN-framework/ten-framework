import asyncio
import json
import time
import os
from typing import Literal

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
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

# Memory store abstraction
from .memory import MemoryStore, MemuSdkMemoryStore, MemuHttpMemoryStore


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

        # Memory related attributes (named memu_client by request)
        self.memu_client: MemoryStore | None = None

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        # Initialize memory store per config toggle
        if self.config.self_hosting:
            self.memu_client = MemuHttpMemoryStore(
                env=ten_env,
                base_url=self.config.memu_base_url,
                api_key=self.config.memu_api_key,
            )
        else:
            self.memu_client = MemuSdkMemoryStore(
                env=ten_env,
                base_url=self.config.memu_base_url,
                api_key=self.config.memu_api_key,
            )

        self.agent = Agent(ten_env)

        # Load memory summary and write into LLM context
        await self._load_memory_to_context()

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
            # Use user's query to search for related memories and pass to LLM
            related_memory = await self._retrieve_related_memory(event.text)
            if related_memory:
                # Add related memory as context to LLM input
                context_message = f"[Related Memory Context]\n{related_memory}\n\n[Current User Question]\n{event.text}"
                await self.agent.queue_llm_input(context_message)
            else:
                await self.agent.queue_llm_input(event.text)
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

            # Memorize every two rounds (when turn_id is even) if memorization is enabled
            if self.turn_id % 2 == 0 and self.config.enable_memorization:
                await self._memorize_conversation()

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

    async def _retrieve_memory(self, user_id: str = None) -> str:
        """Retrieve conversation memory from configured store"""
        if not self.memu_client:
            return ""

        try:
            user_id = self.config.user_id
            agent_id = self.config.agent_id
            resp = await self.memu_client.retrieve_default_categories(
                user_id=user_id, agent_id=agent_id
            )
            normalized = self.memu_client.parse_default_categories(resp)
            return self._extract_summary_text(normalized)
        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to retrieve memory: {e}"
            )
            return ""

    async def _retrieve_related_memory(
        self, query: str, user_id: str = None
    ) -> str:
        """Retrieve related memory based on user query using semantic search"""
        if not self.memu_client:
            return ""

        try:
            user_id = self.config.user_id
            agent_id = self.config.agent_id

            self.ten_env.log_info(
                f"[MainControlExtension] Searching related memory with query: '{query}'"
            )

            # Call semantic search API
            resp = await self.memu_client.retrieve_related_clustered_categories(
                user_id=user_id, agent_id=agent_id, category_query=query
            )

            # Parse response
            parsed = self.memu_client.parse_related_clustered_categories(resp)

            # Extract memory text
            memory_text = self._extract_related_memory_text(parsed)

            self.ten_env.log_info(
                f"[MainControlExtension] Retrieved related memory (length: {len(memory_text)})"
            )

            return memory_text
        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to retrieve related memory: {e}"
            )
            return ""

    def _parse_memory_summary(self, data) -> dict:
        """Parse memory data and create summary"""
        summary = {
            "basic_stats": {
                "total_categories": len(data.categories),
                "total_memories": sum(
                    cat.memory_count or 0 for cat in data.categories
                ),
                "user_id": (
                    data.categories[0].user_id if data.categories else None
                ),
                "agent_id": (
                    data.categories[0].agent_id if data.categories else None
                ),
            },
            "categories": [],
        }

        for category in data.categories:
            cat_summary = {
                "name": category.name,
                "type": category.type,
                "memory_count": category.memory_count,
                "is_active": category.is_active,
                "recent_memories": [],
                "summary": category.summary,
            }

            if category.memories:
                recent = sorted(
                    category.memories, key=lambda x: x.happened_at, reverse=True
                )
                for memory in recent:
                    cat_summary["recent_memories"].append(
                        {
                            "date": memory.happened_at.strftime(
                                "%Y-%m-%d %H:%M"
                            ),
                            "content": memory.content,
                        }
                    )

            summary["categories"].append(cat_summary)

        return summary

    def _extract_summary_text(self, summary: dict) -> str:
        """Extract summary text from parsed memory data"""
        summary_text = ""
        for category in summary["categories"]:
            if category.get("summary"):
                summary_text += category["summary"] + "\n"
            elif category.get("recent_memories"):
                # If no summary, extract content from recent memories
                for memory in category["recent_memories"]:
                    if memory.get("content"):
                        summary_text += f"- {memory['content']}\n"
        result = summary_text.strip()
        self.ten_env.log_info(
            f"[MainControlExtension] _extract_summary_text result: '{result}'"
        )
        return result

    async def _memorize_conversation(
        self, user_id: str = None, user_name: str = None
    ):
        """Memorize the current conversation via configured store"""
        if not self.memu_client:
            return

        try:
            user_id = self.config.user_id
            user_name = self.config.user_name

            # Read context directly from llm_exec
            llm_context = (
                self.agent.llm_exec.get_context()
                if self.agent and self.agent.llm_exec
                else []
            )
            conversation_for_memory = []
            for m in llm_context:
                role = getattr(m, "role", None)
                content = getattr(m, "content", None)
                if role in ["user", "assistant"] and isinstance(content, str):
                    conversation_for_memory.append(
                        {"role": role, "content": content}
                    )

            if not conversation_for_memory:
                return
            asyncio.create_task(
                self.memu_client.memorize(
                    conversation=conversation_for_memory,
                    user_id=user_id,
                    user_name=user_name,
                    agent_id=self.config.agent_id,
                    agent_name=self.config.agent_name,
                )
            )

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to memorize conversation: {e}"
            )

    # Removed: _build_conversation_context (no longer keeping a separate context)

    async def _load_memory_to_context(self):
        """Load memory summary into LLM context at startup (as a system message)."""
        if not self.memu_client:
            return

        try:
            memory_summary = await self._retrieve_memory(self.config.user_id)
            self.ten_env.log_info(
                f"[MainControlExtension] Memory summary: {memory_summary}"
            )
            if memory_summary and self.agent and self.agent.llm_exec:
                # Reset and write memory summary into context as a normal message (no system role handling)
                self.agent.llm_exec.clear_context()
                await self.agent.llm_exec.write_context(
                    self.ten_env,
                    "assistant",
                    "Memory summary of previous conversations:\n\n"
                    + memory_summary,
                )
                self.ten_env.log_info(
                    "[MainControlExtension] Memory summary written into LLM context"
                )
        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to load memory to context: {e}"
            )

    # Removed: _update_llm_context and _sync_context_from_llM (no separate context to sync)

    def _extract_related_memory_text(self, parsed_data: dict) -> str:
        """Extract and format text from related clustered categories search results"""
        if not parsed_data or "categories" not in parsed_data:
            return ""

        parts = []
        query = parsed_data.get("query", "")
        total = parsed_data.get("total_categories", 0)

        if total == 0:
            return ""

        # Add search result header information
        parts.append(
            f"Found {total} related memory categories based on query '{query}':\n"
        )

        # Iterate through each related category
        for cat in parsed_data["categories"]:
            cat_name = cat.get("name", "Unknown Category")
            similarity = cat.get("similarity_score", 0)
            memory_count = cat.get("memory_count", 0)

            # Add category information
            parts.append(
                f"\n【{cat_name}】(Similarity: {similarity:.2f}, Memory Count: {memory_count})"
            )

            # Add category summary
            if cat.get("summary"):
                parts.append(f"  Summary: {cat['summary']}")

            # Add recent memory content
            if cat.get("recent_memories"):
                parts.append("  Recent Memories:")
                for mem in cat["recent_memories"][:3]:  # Only take the first 3
                    date = mem.get("date", "")
                    content = mem.get("content", "")
                    if date and content:
                        parts.append(f"    - [{date}] {content}")
                    elif content:
                        parts.append(f"    - {content}")

        return "\n".join(parts)
