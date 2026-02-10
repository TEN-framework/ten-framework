import asyncio
from datetime import datetime
import json
import time
import logging

logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)

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
from .prompt import (
    CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE,
    PERSONALIZED_GREETING_TEMPLATE,
)

import uuid

# Memory store abstraction
from .memory import (
    MemoryStore,
    PowerMemSdkMemoryStore,
    PowerMemSdkUserMemoryStore,
)


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

        # Memory related attributes (named memory_store by request)
        self.memory_store: MemoryStore | None = None
        self.last_memory_update_turn_id: int = 0

        # Memory idle timer: save memory after 30 seconds of inactivity
        self._memory_idle_timer_task: asyncio.Task | None = None

        # Greeting generation state
        self._is_generating_greeting: bool = False
        self._greeting_future: asyncio.Future[str] | None = None

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
            and self.config.powermem_config
        ):
            try:
                if not self.config.enable_user_memory:
                    self.memory_store = PowerMemSdkMemoryStore(
                        config=self.config.powermem_config, env=ten_env
                    )
                else:
                    self.memory_store = PowerMemSdkUserMemoryStore(
                        config=self.config.powermem_config, env=ten_env
                    )
                ten_env.log_info(
                    "[MainControlExtension] PowerMem memory store initialized successfully"
                )
            except Exception as e:
                ten_env.log_error(
                    f"[MainControlExtension] Failed to initialize PowerMem memory store: {e}. "
                    "The extension will continue without memory functionality."
                )
                import traceback

                ten_env.log_error(
                    f"[MainControlExtension] PowerMem initialization traceback: {traceback.format_exc()}"
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
        if self._rtc_user_count == 1:
            # Generate personalized greeting based on user memories
            personalized_greeting = await self._generate_personalized_greeting()
            if personalized_greeting:
                self.ten_env.log_info(
                    f"[MainControlExtension] Using personalized greeting: {personalized_greeting[:100]}..."
                )
                if self.config:
                    self.config.greeting = personalized_greeting

                await self._send_to_tts(personalized_greeting, True)
                await self._send_transcript(
                    "assistant", personalized_greeting, True, 100
                )
            elif self.config and self.config.greeting:
                self.ten_env.log_info(
                    "[MainControlExtension] No personalized greeting generated, using default greeting"
                )

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
            # Cancel memory idle timer since user started a new conversation
            self._cancel_memory_idle_timer()
            # Use user's query to search for related memories and pass to LLM
            related_memory = await self._retrieve_related_memory(event.text)
            if related_memory:
                # Add related memory as context to LLM input
                context_message = CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE.format(
                    related_memory=related_memory, user_query=event.text
                )
                await self.agent.queue_llm_input(context_message)
            else:
                await self.agent.queue_llm_input(event.text)
        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        # Check if we're generating a personalized greeting
        if self._is_generating_greeting and event.type == "message":
            # For greeting generation, only handle final response
            if event.is_final:
                # Set the future with the greeting text and reset state
                if self._greeting_future and not self._greeting_future.done():
                    self._greeting_future.set_result(event.text)
                self._is_generating_greeting = False
                self._greeting_future = None
                # Don't send greeting to TTS here - it will be sent in _on_user_joined
                # But still send transcript for logging
                await self._send_transcript(
                    "assistant",
                    event.text,
                    event.is_final,
                    100,
                    data_type="text",
                )
            # For non-final events during greeting generation, just log transcript
            # but don't send to TTS (to avoid duplicate output)
            else:
                await self._send_transcript(
                    "assistant",
                    event.text,
                    event.is_final,
                    100,
                    data_type="text",
                )
            return

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

    async def _generate_personalized_greeting(self) -> str:
        """
        Generate a personalized greeting based on user memories.
        Returns an empty string if no memories are found or if generation fails.
        """
        if not self.memory_store or not self.config.enable_memorization:
            return ""

        try:
            # Retrieve user memory summary
            memory_summary = await self.memory_store.get_user_profile(
                user_id=self.config.user_id,
                agent_id=self.config.agent_id,
            )

            if not memory_summary or not memory_summary.strip():
                # No memories found, return empty to use default greeting
                return ""

            # Construct prompt for personalized greeting
            greeting_prompt = PERSONALIZED_GREETING_TEMPLATE.format(
                memory_summary=memory_summary
            )

            # Create a future to wait for the greeting response
            self._greeting_future = asyncio.Future()
            self._is_generating_greeting = True

            # Queue the greeting prompt to LLM
            await self.agent.queue_llm_input(greeting_prompt)

            # Wait for the greeting response (with timeout)
            try:
                greeting = await asyncio.wait_for(
                    self._greeting_future, timeout=10.0
                )
                self.ten_env.log_info(
                    f"[MainControlExtension] Generated personalized greeting: {greeting}"
                )
                return greeting
            except asyncio.TimeoutError:
                # Timeout - cancel the future and reset state
                self.ten_env.log_warn(
                    "[MainControlExtension] Greeting generation timed out"
                )
                if not self._greeting_future.done():
                    self._greeting_future.cancel()
                self._is_generating_greeting = False
                self._greeting_future = None
                return ""

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Failed to generate personalized greeting: {e}"
            )
            self._is_generating_greeting = False
            if self._greeting_future and not self._greeting_future.done():
                self._greeting_future.cancel()
            self._greeting_future = None
            return ""

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
            memorise = [
                result["memory"]
                for result in results
                if isinstance(result, dict) and result.get("memory")
            ]

            # Format memory text using join for better performance
            if memorise:
                memory_text = (
                    "Memorise:\n"
                    + "\n".join(f"- {memory}" for memory in memorise)
                    + "\n"
                )
            else:
                memory_text = ""

            self.ten_env.log_info(
                f"[MainControlExtension] Retrieved related memory (length: {len(memory_text)})"
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
