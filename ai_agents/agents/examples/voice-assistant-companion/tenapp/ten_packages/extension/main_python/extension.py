import asyncio
import time
import os
import json
import aiohttp

from ten_ai_base.mllm import (
    DATA_MLLM_IN_CREATE_RESPONSE,
    DATA_MLLM_IN_SEND_MESSAGE_ITEM,
    DATA_MLLM_IN_SET_MESSAGE_CONTEXT,
)
from ten_ai_base.struct import (
    MLLMClientCreateResponse,
    MLLMClientMessageItem,
    MLLMClientSendMessageItem,
    MLLMClientSetMessageContext,
)
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
)

from .agent.agent import Agent
from .agent.events import (
    FunctionCallEvent,
    HTTPRequestEvent,
    InputTranscriptEvent,
    OutputTranscriptEvent,
    SaveMemoryEvent,
    ServerInterruptEvent,
    SessionReadyEvent,
    ToolRegisterEvent,
    UserJoinedEvent,
    UserLeftEvent,
)
from .helper import _send_cmd, _send_data
from .config import MainControlConfig  # assume extracted from your base model


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
        self.session_ready: bool = False
        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.current_metadata: dict = {"session_id": "0"}

        # Memu Memory integration
        self.memu_api_key: str = os.getenv("MEMU_API_KEY", "")
        self.memu_base_url: str = "https://api.memu.so"
        self.conversation_history: list = []
        self.user_id: str = "default_user"
        self.agent_id: str = "companion_001"

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        self.agent = Agent(ten_env)

        # Start agent event loop
        asyncio.create_task(self._consume_agent_events())

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")
        # Set initial context messages if needed
        # This can be customized based on your application's needs
        # For example, you might want to set a greeting message or initial context

        # await self._set_context_messages(
        #     messages=[
        #         MLLMClientMessageItem(role="user", content=f"What's the weather like today?"),
        #         MLLMClientMessageItem(role="assistant", content=f"It's rainning today"),
        #     ]
        # )

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True

        # Save conversation to Memu before stopping (insurance in case UserLeftEvent is missed)
        if self.conversation_history and self.memu_api_key:
            ten_env.log_info(
                "[MainControlExtension] on_stop: Saving conversation to Memu"
            )
            await self._save_memory_to_memu()

        if self.agent:
            await self.agent.stop()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        await self.agent.on_data(data)

    async def _consume_agent_events(self):
        """
        Main event loop that consumes semantic AgentEvents from the Agent class.
        Dispatches logic based on event type and name.
        """
        while not self.stopped:
            try:
                event = await self.agent.get_event()

                match event:
                    case UserJoinedEvent():
                        self._rtc_user_count += 1
                        await self._greeting_if_ready()

                    case UserLeftEvent():
                        self._rtc_user_count -= 1
                        self.ten_env.log_info(
                            f"[MainControlExtension] User left, rtc_user_count={self._rtc_user_count}"
                        )
                        self.ten_env.log_info(
                            f"[MainControlExtension] Conversation history length: {len(self.conversation_history)}"
                        )
                        self.ten_env.log_info(
                            f"[MainControlExtension] Memu API key set: {bool(self.memu_api_key)}"
                        )

                        # Save conversation to Memu when user leaves
                        if self.conversation_history and self.memu_api_key:
                            self.ten_env.log_info(
                                "[MainControlExtension] Triggering Memu save..."
                            )
                            await self._save_memory_to_memu()
                        else:
                            if not self.conversation_history:
                                self.ten_env.log_warn(
                                    "[MainControlExtension] No conversation history to save"
                                )
                            if not self.memu_api_key:
                                self.ten_env.log_warn(
                                    "[MainControlExtension] Memu API key not set"
                                )

                    case SaveMemoryEvent():
                        self.ten_env.log_info(
                            "[MainControlExtension] Received save_memory command"
                        )
                        if self.conversation_history and self.memu_api_key:
                            self.ten_env.log_info(
                                "[MainControlExtension] Saving conversation to Memu..."
                            )
                            await self._save_memory_to_memu()
                        else:
                            if not self.conversation_history:
                                self.ten_env.log_info(
                                    "[MainControlExtension] No conversation history to save"
                                )
                            if not self.memu_api_key:
                                self.ten_env.log_warn(
                                    "[MainControlExtension] Memu API key not set"
                                )

                    case HTTPRequestEvent():
                        self.ten_env.log_info(
                            f"[MainControlExtension] Received HTTP request: {event.body}"
                        )
                        # Handle different HTTP commands based on body content
                        if "name" in event.body:
                            cmd_name = event.body.get("name")
                            if cmd_name == "save_memory":
                                self.ten_env.log_info(
                                    "[MainControlExtension] HTTP request to save memory"
                                )
                                if (
                                    self.conversation_history
                                    and self.memu_api_key
                                ):
                                    await self._save_memory_to_memu()
                                else:
                                    if not self.conversation_history:
                                        self.ten_env.log_info(
                                            "[MainControlExtension] No conversation history to save"
                                        )
                                    if not self.memu_api_key:
                                        self.ten_env.log_warn(
                                            "[MainControlExtension] Memu API key not set"
                                        )
                            elif (
                                cmd_name == "message"
                                and "payload" in event.body
                            ):
                                # Handle text message from HTTP
                                text = event.body.get("payload", {}).get(
                                    "text", ""
                                )
                                if text:
                                    self.ten_env.log_info(
                                        f"[MainControlExtension] HTTP message: {text}"
                                    )
                                    # You can process the text message here if needed

                    case ToolRegisterEvent():
                        await self.agent.register_tool(event.tool, event.source)
                    case FunctionCallEvent():
                        await self.agent.call_tool(
                            event.call_id, event.function_name, event.arguments
                        )
                    case InputTranscriptEvent():
                        self.current_metadata = {
                            "session_id": event.metadata.get(
                                "session_id", "100"
                            ),
                        }
                        stream_id = int(event.metadata.get("session_id", "100"))

                        if event.content == "":
                            self.ten_env.log_info(
                                "[MainControlExtension] Empty ASR result, skipping"
                            )
                            continue

                        # Print user input
                        self.ten_env.log_info(
                            f"[USER] {event.content} (final={event.final})"
                        )

                        # Record user message in conversation history
                        if event.final:
                            self.ten_env.log_info(
                                f"[MainControlExtension] Recording user message: {event.content}"
                            )
                            self.conversation_history.append(
                                {"role": "user", "content": event.content}
                            )

                        await self._send_transcript(
                            role="user",
                            text=event.content,
                            final=event.final,
                            stream_id=stream_id,
                        )

                    case OutputTranscriptEvent():
                        # Handle LLM response events
                        # Print assistant response
                        self.ten_env.log_info(
                            f"[ASSISTANT] {event.content} (final={event.is_final})"
                        )

                        # Record assistant message in conversation history
                        if event.is_final:
                            self.ten_env.log_info(
                                f"[MainControlExtension] Recording assistant message: {event.content}"
                            )
                            self.conversation_history.append(
                                {"role": "assistant", "content": event.content}
                            )

                        await self._send_transcript(
                            role="assistant",
                            text=event.content,
                            final=event.is_final,
                            stream_id=100,
                        )
                    case ServerInterruptEvent():
                        # Handle server interrupt events
                        await self._interrupt()
                    case SessionReadyEvent():
                        # Handle session ready events
                        self.ten_env.log_info(
                            f"[MainControlExtension] Session ready with metadata: {self.current_metadata}"
                        )
                        self.session_ready = True
                        await self._greeting_if_ready()
                    case _:
                        self.ten_env.log_warn(
                            f"[MainControlExtension] Unhandled event: {event}"
                        )

            except Exception as e:
                self.ten_env.log_error(
                    f"[MainControlExtension] Event processing error: {e}"
                )

    async def _greeting_if_ready(self):
        """
        Sends a greeting message if the agent is ready and the user count is 1.
        This is typically called when the first user joins.
        Retrieves user memories from Memu and includes them in the greeting context.
        """
        if (
            self._rtc_user_count == 1
            and self.config.greeting
            and self.session_ready
        ):
            # Retrieve memories from Memu
            memory_context = await self._retrieve_memories_from_memu()
            self.ten_env.log_info(
                f"[MainControlExtension] Memory context retrieved: {memory_context[:200] if memory_context else 'None'}..."
            )

            # Prepare greeting message with memory context if available
            greeting_message = f"say {self.config.greeting} to me"

            if memory_context:
                self.ten_env.log_info(
                    "[MainControlExtension] Including memory context in greeting"
                )
                # Prepend memory context to the greeting message
                # This ensures the AI sees the context before responding
                greeting_message = (
                    f"{memory_context}\n\nNow, {greeting_message}"
                )
                self.ten_env.log_info(
                    f"[MainControlExtension] Full greeting with context: {greeting_message[:300]}..."
                )
            else:
                self.ten_env.log_info(
                    "[MainControlExtension] No memory context available from Memu"
                )

            # Send greeting with memory context embedded
            await self._send_message_item(
                MLLMClientMessageItem(
                    role="user",
                    content=greeting_message,
                )
            )
            await self._send_create_response()
            self.ten_env.log_info(
                "[MainControlExtension] Sent greeting message with memory context"
            )

    async def _send_transcript(
        self, role: str, text: str, final: bool, stream_id: int
    ):
        """
        Sends the transcript (ASR or LLM output) to the message collector.
        """
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
        self.ten_env.log_info(
            f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
        )

    async def _set_context_messages(
        self, messages: list[MLLMClientMessageItem]
    ):
        """
        Set the context messages for the LLM.
        This method sends a command to set the provided messages.
        """
        await _send_data(
            self.ten_env,
            DATA_MLLM_IN_SET_MESSAGE_CONTEXT,
            "v2v",
            MLLMClientSetMessageContext(messages=messages).model_dump(),
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Set context messages: {len(messages)} items"
        )

    async def _send_message_item(self, message: MLLMClientMessageItem):
        """
        Send a message to the LLM.
        This method sends a command to send the provided message item.
        """
        await _send_data(
            self.ten_env,
            DATA_MLLM_IN_SEND_MESSAGE_ITEM,
            "v2v",
            MLLMClientSendMessageItem(message=message).model_dump(),
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent message: {message.content} from {message.role}"
        )

    async def _send_create_response(self):
        """
        Create a response in the LLM.
        This method sends a command to create a response.
        """
        await _send_data(
            self.ten_env,
            DATA_MLLM_IN_CREATE_RESPONSE,
            "v2v",
            MLLMClientCreateResponse().model_dump(),
        )
        self.ten_env.log_info("[MainControlExtension] Created LLM response")

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")

    async def _save_memory_to_memu(self):
        """
        Save conversation history to Memu memory service.
        Called when user leaves the conversation.
        """
        if not self.memu_api_key:
            self.ten_env.log_info(
                "[MainControlExtension] Memu API key not set, skipping memory save"
            )
            return

        if not self.conversation_history:
            self.ten_env.log_info(
                "[MainControlExtension] No conversation history to save"
            )
            return

        try:
            self.ten_env.log_info(
                f"[MainControlExtension] Saving {len(self.conversation_history)} messages to Memu"
            )

            async with aiohttp.ClientSession() as session:
                # Correct Memu API endpoint based on official documentation
                url = f"{self.memu_base_url}/api/v1/memory/memorize"
                headers = {
                    "Authorization": f"Bearer {self.memu_api_key}",
                    "Content-Type": "application/json",
                }

                # Format according to Memu API documentation
                payload = {
                    "conversation": self.conversation_history,
                    "user_id": self.user_id,
                    "user_name": "User",
                    "agent_id": self.agent_id,
                    "agent_name": "AI Companion",
                }

                self.ten_env.log_info(
                    f"[MainControlExtension] Calling Memu API: {url}"
                )

                async with session.post(
                    url, json=payload, headers=headers
                ) as response:
                    response_text = await response.text()
                    if response.status == 200:
                        result = await response.json() if response_text else {}
                        task_id = result.get("task_id", "N/A")
                        status = result.get("status", "N/A")
                        self.ten_env.log_info(
                            f"[MainControlExtension] Successfully submitted to Memu - Task ID: {task_id}, Status: {status}"
                        )
                        # Clear conversation history after successful save
                        self.conversation_history = []
                    else:
                        self.ten_env.log_error(
                            f"[MainControlExtension] Failed to save to Memu: {response.status} - {response_text}"
                        )

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Error saving to Memu: {str(e)}"
            )

    async def _retrieve_memories_from_memu(self):
        """
        Retrieve user memories from Memu to provide context.
        Returns a formatted string of user memories.
        """
        self.ten_env.log_info(
            "[MainControlExtension] Starting memory retrieval from Memu"
        )

        if not self.memu_api_key:
            self.ten_env.log_warn(
                "[MainControlExtension] Memu API key not set, skipping retrieval"
            )
            return ""

        try:
            self.ten_env.log_info(
                f"[MainControlExtension] Calling Memu API for user_id={self.user_id}, agent_id={self.agent_id}"
            )

            async with aiohttp.ClientSession() as session:
                url = f"{self.memu_base_url}/api/v1/memory/retrieve/default-categories"
                headers = {
                    "Authorization": f"Bearer {self.memu_api_key}",
                    "Content-Type": "application/json",
                }
                payload = {
                    "user_id": self.user_id,
                    "agent_id": self.agent_id,
                    "want_memory_items": False,  # Get summaries
                }

                self.ten_env.log_info(
                    f"[MainControlExtension] Memu API URL: {url}"
                )
                self.ten_env.log_info(
                    f"[MainControlExtension] Memu API payload: {payload}"
                )

                async with session.post(
                    url, json=payload, headers=headers
                ) as response:
                    self.ten_env.log_info(
                        f"[MainControlExtension] Memu API response status: {response.status}"
                    )

                    if response.status == 200:
                        result = await response.json()
                        self.ten_env.log_info(
                            f"[MainControlExtension] Memu API response: {result}"
                        )

                        categories = result.get("categories", [])

                        if not categories:
                            self.ten_env.log_info(
                                "[MainControlExtension] No memories found in Memu"
                            )
                            return ""

                        # Format memories as context for the AI
                        memory_context = "## Your Memory About This User\n\n"
                        memory_context += "You have conversed with this user before. Here's what you remember:\n\n"

                        memory_count = 0
                        for category in categories:
                            summary = category.get("summary", "").strip()
                            if summary:
                                category_name = (
                                    category.get("name", "")
                                    .replace("_", " ")
                                    .title()
                                )
                                memory_context += (
                                    f"- **{category_name}**: {summary}\n"
                                )
                                memory_count += 1
                                self.ten_env.log_info(
                                    f"[MainControlExtension] Found memory: {category_name} = {summary}"
                                )

                        if memory_count == 0:
                            self.ten_env.log_info(
                                "[MainControlExtension] No summaries found in categories"
                            )
                            return ""

                        memory_context += "\n## Important Instructions\n"
                        memory_context += "- Use this information naturally in your conversation\n"
                        memory_context += (
                            "- Reference past conversations when relevant\n"
                        )
                        memory_context += "- Show that you remember the user\n"
                        memory_context += '- Don\'t explicitly say "I\'m retrieving memories" or "according to my records"\n'
                        memory_context += (
                            "- Act as if you naturally recall these details\n"
                        )

                        self.ten_env.log_info(
                            f"[MainControlExtension] Successfully retrieved {memory_count} memory categories from Memu"
                        )
                        self.ten_env.log_info(
                            f"[MainControlExtension] Full memory context:\n{memory_context}"
                        )
                        return memory_context
                    else:
                        response_text = await response.text()
                        self.ten_env.log_warn(
                            f"[MainControlExtension] Failed to retrieve from Memu: {response.status} - {response_text}"
                        )
                        return ""

        except Exception as e:
            self.ten_env.log_error(
                f"[MainControlExtension] Error retrieving from Memu: {str(e)}"
            )
            import traceback

            self.ten_env.log_error(
                f"[MainControlExtension] Traceback: {traceback.format_exc()}"
            )
            return ""
