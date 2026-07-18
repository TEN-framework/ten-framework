import asyncio
from datetime import datetime
import json
import time
import logging

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logging.getLogger("websockets").setLevel(logging.WARNING)

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
from .config import MainControlConfig

import uuid

# Memory store abstraction
from .memory import MemoryStore, EverMemosMemoryStore


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module with EverMemOS integration.
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

        # Memory related attributes
        self.memory_store: MemoryStore | None = None
        self.last_memory_update_turn_id: int = 0

        # Memory idle timer: save memory after N seconds of inactivity
        self._memory_idle_timer_task: asyncio.Task | None = None

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        self.ten_env.log_info(f"[MainControlExtension] config={self.config}")

        # Initialize memory store
        if (
            self.config
            and self.config.enable_memorization
            and self.config.evermemos_config
        ):
            try:
                self.memory_store = EverMemosMemoryStore(
                    config=self.config.evermemos_config, env=ten_env
                )
                ten_env.log_info(
                    "[MainControlExtension] EverMemOS memory store initialized successfully"
                )
            except Exception as e:
                ten_env.log_error(
                    f"[MainControlExtension] Failed to initialize EverMemOS memory store: {e}. "
                    "The extension will continue without memory functionality."
                )
                import traceback

                ten_env.log_error(
                    f"[MainControlExtension] EverMemOS initialization traceback: {traceback.format_exc()}"
                )
                self.memory_store = None

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
            # 使用配置文件中的问候语
            greeting = self.config.greeting

            self.ten_env.log_info(
                f"[MainControlExtension] Using greeting from config: {greeting}"
            )

            await self._send_to_tts(greeting, True)
            await self._send_transcript("assistant", greeting, True, 100)

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
            # Cancel memory idle timer since user started a new conversation
            self._cancel_memory_idle_timer()

            # Use user's query to search for related memories and pass to LLM
            related_memory = await self._retrieve_related_memory(event.text)

            if related_memory:
                # ✅ 使用简洁格式注入记忆，无需模板包装
                context_message = (
                    f"[相关记忆]\n{related_memory}\n\n"
                    f"[用户问题]\n{event.text}"
                )
                await self.agent.queue_llm_input(context_message)
            else:
                await self.agent.queue_llm_input(event.text)
        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        # Normal LLM response handling
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

            # Memorize every N rounds if memorization is enabled
            if (
                self.turn_id - self.last_memory_update_turn_id
                >= self.config.memory_save_interval_turns
                and self.config.enable_memorization
            ):
                # Update counter immediately to prevent race condition from concurrent saves
                # This ensures only one save task is triggered even if multiple responses arrive quickly
                current_turn_id = self.turn_id
                self.last_memory_update_turn_id = current_turn_id
                # Save memory asynchronously without blocking LLM response processing
                asyncio.create_task(self._memorize_conversation())
                # Cancel idle timer since we just saved memory
                self._cancel_memory_idle_timer()
            elif self.config.enable_memorization:
                # Start/reset idle timer to save memory if no new conversation
                self._start_memory_idle_timer()

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
        # Cancel idle timer before stopping
        self._cancel_memory_idle_timer()
        await self.agent.stop()
        await self._memorize_conversation()

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

    def _cancel_memory_idle_timer(self):
        """Cancel the memory idle timer if it exists"""
        if (
            self._memory_idle_timer_task
            and not self._memory_idle_timer_task.done()
        ):
            self._memory_idle_timer_task.cancel()
            self._memory_idle_timer_task = None
            self.ten_env.log_info(
                "[MainControlExtension] Cancelled memory idle timer"
            )

    def _start_memory_idle_timer(self):
        """Start or reset the 30-second idle timer to save memory"""
        # Cancel existing timer if any
        self._cancel_memory_idle_timer()

        async def _memory_idle_timeout():
            """Wait for configured idle timeout and then save memory if there are unsaved conversations"""
            # Capture reference to this task to avoid race condition
            current_task = asyncio.current_task()
            timeout_seconds = self.config.memory_idle_timeout_seconds
            try:
                await asyncio.sleep(timeout_seconds)
                # Check if there are unsaved conversations
                if (
                    self.turn_id > self.last_memory_update_turn_id
                    and self.config.enable_memorization
                    and not self.stopped
                ):
                    self.ten_env.log_info(
                        f"[MainControlExtension] {timeout_seconds} seconds idle timeout reached, "
                        f"saving memory (turn_id={self.turn_id}, "
                        f"last_saved_turn_id={self.last_memory_update_turn_id})"
                    )
                    await self._memorize_conversation()
                # Only clear if this task is still the current timer task
                if self._memory_idle_timer_task is current_task:
                    self._memory_idle_timer_task = None
            except asyncio.CancelledError:
                # Timer was cancelled, which is expected
                # Only clear if this task is still the current timer task
                if self._memory_idle_timer_task is current_task:
                    self._memory_idle_timer_task = None
            except Exception as e:
                self.ten_env.log_error(
                    f"[MainControlExtension] Error in memory idle timer: {e}"
                )
                # Only clear if this task is still the current timer task
                if self._memory_idle_timer_task is current_task:
                    self._memory_idle_timer_task = None

        # Start new timer task
        self._memory_idle_timer_task = asyncio.create_task(
            _memory_idle_timeout()
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Started {self.config.memory_idle_timeout_seconds}-second memory idle timer"
        )

    async def _retrieve_related_memory(self, query: str) -> str:
        """Retrieve related memory based on user query using semantic search"""
        if not self.memory_store:
            return ""

        try:
            user_id = self.config.user_id
            agent_id = self.config.agent_id

            self.ten_env.log_info(
                f"[MainControlExtension] Searching related memory with query: '{query}'"
            )

            # Call semantic search API
            resp = await self.memory_store.search(
                user_id=user_id, agent_id=agent_id, query=query
            )

            if not resp or not isinstance(resp, dict):
                return ""

            # Extract memory content from results using list comprehension
            results = resp.get("results", [])

            # ✅ 限制只取前 3 条最相关的记忆（优化性能）
            memorise = [
                result["memory"]
                for result in results[:3]  # 只取前3条
                if isinstance(result, dict) and result.get("memory")
            ]

            # ✅ 使用更简洁的格式
            if memorise:
                memory_parts = []
                for i, memory in enumerate(memorise, 1):
                    # 提取分数和时间戳（如果有）
                    result = results[i - 1]
                    score = result.get("score", "N/A")
                    timestamp = result.get("timestamp", "")

                    memory_parts.append(f"{i}. {memory}")
                    if score != "N/A" or timestamp:
                        memory_parts.append(
                            f"   (相关度: {score}, 时间: {timestamp})"
                        )

                memory_text = "\n".join(memory_parts)
            else:
                memory_text = ""

            self.ten_env.log_info(
                f"[MainControlExtension] Retrieved {len(memorise)} related memories (total length: {len(memory_text)})"
            )

            return memory_text
        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to retrieve related memory: {e}"
            )
            return ""

    async def _memorize_conversation(self):
        """Memorize the current conversation via configured store"""
        if not self.memory_store:
            return

        try:
            user_id = self.config.user_id

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

            await self.memory_store.add(
                conversation=conversation_for_memory,
                user_id=user_id,
                agent_id=self.config.agent_id,
            )
            # Update counter if not already updated (for on_stop and idle timeout cases)
            # For LLM response case, counter is updated before creating the task to prevent race conditions
            if self.turn_id > self.last_memory_update_turn_id:
                self.last_memory_update_turn_id = self.turn_id

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to memorize conversation: {e}"
            )
