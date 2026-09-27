import json
import time
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
    ModelRouteEvent,
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
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"
        self._asr_final_segments: list[str] = []
        self._asr_interim: str = ""

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        self.agent = Agent(ten_env, self.config.model_routing)

        # Now auto-register decorated methods
        for attr_name in dir(self):
            fn = getattr(self, attr_name)
            event_type = getattr(fn, "_agent_event_type", None)
            if event_type:
                self.agent.on(event_type, fn)

    # === Register handlers with decorators ===
    @agent_event_handler(UserJoinedEvent)
    async def _on_user_joined(self, _event: UserJoinedEvent):
        self._rtc_user_count += 1
        if self._rtc_user_count == 1 and self.config and self.config.greeting:
            await self._send_to_tts(self.config.greeting, True)
            await self._send_transcript(
                "assistant", self.config.greeting, True, 100
            )

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, _event: UserLeftEvent):
        self._rtc_user_count -= 1

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
        session_id = event.metadata.get("session_id", "100")
        if (
            self.config.turn_detection_mode == "speech_final"
            and session_id != self.session_id
        ):
            self._asr_final_segments.clear()
            self._asr_interim = ""
        self.session_id = session_id
        stream_id = int(self.session_id)
        if self.config.turn_detection_mode == "speech_final":
            await self._on_asr_result_speech_final(event, stream_id)
            return
        if not event.text:
            return
        if event.final or len(event.text) > 2:
            await self._interrupt()
        if event.final:
            self.turn_id += 1
            await self.agent.queue_llm_input(event.text)
        await self._send_transcript("user", event.text, event.final, stream_id)

    async def _on_asr_result_speech_final(
        self, event: ASRResultEvent, stream_id: int
    ):
        asr_info = event.metadata.get("asr_info") or {}
        speech_final = (
            isinstance(asr_info, dict) and asr_info.get("speech_final") is True
        )
        if event.text:
            # Keep the existing barge-in policy separate from turn detection.
            if event.final or len(event.text) > 2:
                await self._interrupt()
            if event.final:
                self._asr_final_segments.append(event.text)
                self._asr_interim = ""
            else:
                self._asr_interim = event.text

        segments = self._asr_final_segments.copy()
        if self._asr_interim:
            segments.append(self._asr_interim)
        utterance = " ".join(
            segment.strip() for segment in segments if segment.strip()
        )

        if speech_final:
            self._asr_final_segments.clear()
            self._asr_interim = ""
            if utterance:
                self.turn_id += 1
                await self.agent.queue_llm_input(utterance)
                await self._send_transcript("user", utterance, True, stream_id)
        elif event.text and utterance:
            await self._send_transcript("user", utterance, False, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        # The collector queues every snapshot as a new message. Send the
        # complete reasoning once so a long chain cannot delay the answer.
        if event.type == "reasoning" and not event.is_final:
            return
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

        await self._send_transcript(
            "assistant",
            event.text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    @agent_event_handler(ModelRouteEvent)
    async def _on_model_route(self, event: ModelRouteEvent):
        if event.choice:
            confidence = (
                f" (confidence {event.confidence:.2f})"
                if event.confidence is not None
                else ""
            )
            result = f"{event.choice}{confidence}"
        else:
            result = event.status.replace("_", " ")
        fallback = (
            " (fallback)"
            if event.status
            not in (
                "selected_fast",
                "selected_deep",
            )
            else ""
        )
        mode = (
            "Flash · thinking high"
            if event.destination == self.config.model_routing.deep_dest
            else "Flash · no thinking"
        )
        label = (
            f"Jev route: {result} → {mode}{fallback} · {event.latency_ms} ms"
        )
        await _send_data(
            self.ten_env,
            "message",
            "message_collector",
            {
                "data_type": "raw",
                "role": "assistant",
                "text": json.dumps(
                    {"type": "routing", "data": {"text": label}}
                ),
                "text_ts": int(time.time() * 1000),
                "is_final": True,
                "stream_id": 100,
            },
        )

    async def on_start(self, _ten_env: AsyncTenEnv):
        self.ten_env.log_info("[MainControlExtension] on_start")

    async def on_stop(self, _ten_env: AsyncTenEnv):
        self.ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

    async def on_cmd(self, _ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, _ten_env: AsyncTenEnv, data: Data):
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
            f"[MainControlExtension] Sent transcript: role={role}, "
            f"type={data_type}, final={final}, length={len(text)}"
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
