import asyncio
import time

from ten_ai_base.mllm import DATA_MLLM_IN_CREATE_RESPONSE, DATA_MLLM_IN_SEND_MESSAGE_ITEM, DATA_MLLM_IN_SET_MESSAGE_CONTEXT
from ten_ai_base.struct import MLLMClientCreateResponse, MLLMClientMessageItem, MLLMClientSendMessageItem, MLLMClientSetMessageContext
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
)

from .agent.agent import Agent
from .agent.events import (
    FunctionCallEvent,
    InputTranscriptEvent,
    OutputTranscriptEvent,
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
        self.current_metadata: dict = { "session_id": "0" }

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

                    case ToolRegisterEvent():
                        await self.agent.register_tool(event.tool, event.source)
                    case FunctionCallEvent():
                        await self.agent.call_tool(event.call_id, event.function_name, event.arguments)
                    case InputTranscriptEvent():
                        self.current_metadata = {
                            "session_id": event.metadata.get("session_id", "100"),
                        }
                        stream_id = int(event.metadata.get("session_id", "100"))

                        if event.content == "":
                            self.ten_env.log_info("[MainControlExtension] Empty ASR result, skipping")
                            continue


                        await self._send_transcript(
                            role="user",
                            text=event.content,
                            final=event.final,
                            stream_id=stream_id,
                        )

                    case OutputTranscriptEvent():
                        # Handle LLM response events
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
                        self.ten_env.log_info(f"[MainControlExtension] Session ready with metadata: {self.current_metadata}")
                        self.session_ready = True
                        await self._greeting_if_ready()
                    case _:
                        self.ten_env.log_warn(f"[MainControlExtension] Unhandled event: {event}")

            except Exception as e:
                self.ten_env.log_error(f"[MainControlExtension] Event processing error: {e}")

    async def _greeting_if_ready(self):
        """
        Sends a greeting message if the agent is ready and the user count is 1.
        This is typically called when the first user joins.
        """
        if self._rtc_user_count == 1 and self.config.greeting and self.session_ready:
            await self._send_message_item(MLLMClientMessageItem(
                    role="user",
                    content=f"say {self.config.greeting} to me",
            ))
            await self._send_create_response()
            self.ten_env.log_info("[MainControlExtension] Sent greeting message")

    async def _send_transcript(self, role: str, text: str, final: bool, stream_id: int):
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
        self.ten_env.log_info(f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}")


    async def _set_context_messages(self, messages: list[MLLMClientMessageItem]):
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
        self.ten_env.log_info(f"[MainControlExtension] Set context messages: {len(messages)} items")

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
        self.ten_env.log_info(f"[MainControlExtension] Sent message: {message.content} from {message.role}")

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
