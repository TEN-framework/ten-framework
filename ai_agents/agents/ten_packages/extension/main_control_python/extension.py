import asyncio
import time

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
        self.current_metadata: dict = { "session_id": "0", "turn_id": -1 }
        self.sentence_fragment: str = ""

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
                        if self._rtc_user_count == 1 and self.config.greeting:
                            await self._send_to_tts(self.config.greeting, is_final=True)
                            await self._send_transcript(
                                role="assistant",
                                text=self.config.greeting,
                                final=True,
                                stream_id=100,
                            )

                    case UserLeftEvent():
                        self._rtc_user_count -= 1

                    case ToolRegisterEvent():
                        await self.agent.register_llm_tool(
                            event.tool,
                            source=event.source,
                        )

                    case ASRResultEvent():
                        self.current_metadata = {
                            "session_id": event.metadata.get("session_id", "100"),
                            "turn_id": event.metadata.get("turn_id", -1),
                        }
                        stream_id = int(event.metadata.get("session_id", "100"))

                        if event.final or len(event.text) > 2:
                            await self._interrupt()

                        if event.final:
                            await self.agent.queue_llm_input(event.text)

                        await self._send_transcript(
                            role="user",
                            text=event.text,
                            final=event.final,
                            stream_id=stream_id,
                        )

                    case LLMResponseEvent():
                        # Handle LLM response events
                        if not event.is_final:
                            sentences, self.sentence_fragment = parse_sentences(
                                self.sentence_fragment, event.delta
                            )
                            for sentence in sentences:
                                await self._send_to_tts(sentence, is_final=False)

                        await self._send_transcript(
                            role="assistant",
                            text=event.text,
                            final=event.is_final,
                            stream_id=100,
                        )
                    case _:
                        self.ten_env.log_warn(f"[MainControlExtension] Unhandled event: {event}")

            except Exception as e:
                self.ten_env.log_error(f"[MainControlExtension] Event processing error: {e}")

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

    async def _send_to_tts(self, text: str, is_final: bool):
        """
        Sends a sentence to the TTS system.
        """
        request_id = str(uuid.uuid4())
        await _send_data(
            self.ten_env,
            "tts_text_input",
            "tts",
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": is_final,
                "metadata": self.current_metadata,
            },
        )
        self.ten_env.log_info(f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}")

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        await self.agent.flush_llm()
        await _send_cmd(self.ten_env, "flush", "tts")
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")
