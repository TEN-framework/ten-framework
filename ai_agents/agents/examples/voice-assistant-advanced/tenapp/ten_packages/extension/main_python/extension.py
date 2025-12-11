import asyncio
import json
import time
from typing import Literal

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    CmdResult,
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
        # Track recently sent TTS content to prevent duplicates (text -> timestamp)
        self._recent_tts_content: dict[str, float] = {}
        # Separate cache for transcript deduplication
        self._recent_transcript_content: dict[str, float] = {}
        self._tts_dedup_window_seconds: float = (
            3.0  # Skip duplicates within this window
        )

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

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

        # Interrupt on INTERIM results too (when user starts speaking)
        if not event.final and len(event.text) >= 2:
            self.ten_env.log_info(
                f'[INTERIM_INTERRUPT] User speaking, interrupting avatar: "{event.text[:30]}..."'
            )
            await self._interrupt()

        if event.final:
            stt_final_time = time.time()
            self.ten_env.log_info(
                f'[LATENCY_STT_FINAL] t={stt_final_time:.3f} text="{event.text}"'
            )
            await self._interrupt()
            self.turn_id += 1
            await self.agent.queue_llm_input(event.text)
        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        if not event.is_final and event.type == "message":
            # Log first LLM response chunk
            if not hasattr(
                self, "_logged_llm_first_token"
            ) or self.turn_id != getattr(self, "_last_logged_turn", -1):
                llm_first_token_time = time.time()
                self.ten_env.log_info(
                    f"[LATENCY_LLM_FIRST_TOKEN] t={llm_first_token_time:.3f} turn={self.turn_id}"
                )
                self._logged_llm_first_token = True
                self._last_logged_turn = self.turn_id

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

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        cmd_name = cmd.get_name()
        if cmd_name == "flush":
            ten_env.log_debug("[MainControlExtension] Received flush from STT")
            await self._interrupt()
            await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))
            return
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        # Handle text_data exactly like ASR: interrupt ongoing speech, increment turn, queue to LLM
        if data.get_name() == "text_data":
            await self._interrupt()  # Stop ongoing TTS/LLM, just like ASR does
            self.turn_id += 1
            ten_env.log_info(
                f"[MainControlExtension] text_data received, interrupted and turn_id incremented to {self.turn_id}"
            )

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
        # Deduplicate final assistant transcripts using separate cache from TTS
        if role == "assistant" and final and data_type == "text" and text:
            current_time = time.time()
            normalized = text.strip().lower()
            if normalized in self._recent_transcript_content:
                last_sent = self._recent_transcript_content[normalized]
                if current_time - last_sent < self._tts_dedup_window_seconds:
                    self.ten_env.log_info(
                        f'[TRANSCRIPT_DEDUP] Skipping duplicate final transcript: "{text}"'
                    )
                    return
            # Track this transcript
            self._recent_transcript_content[normalized] = current_time
            # Clean up old entries
            self._recent_transcript_content = {
                k: v
                for k, v in self._recent_transcript_content.items()
                if current_time - v < self._tts_dedup_window_seconds * 2
            }

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
        current_time = time.time()

        # Skip empty text, but still send end signal if is_final
        if not text or not text.strip():
            if is_final:
                # Send empty text with end signal to properly close TTS stream
                request_id = f"tts-request-{self.turn_id}"
                await _send_data(
                    self.ten_env,
                    "tts_text_input",
                    "tts",
                    {
                        "request_id": request_id,
                        "text": "",
                        "text_input_end": True,
                        "metadata": self._current_metadata(),
                    },
                )
                self.ten_env.log_info(
                    f"[MainControlExtension] Sent TTS end signal (empty text)"
                )
            return

        # Deduplicate: skip if same text was sent recently
        normalized_text = text.strip().lower()
        if normalized_text in self._recent_tts_content:
            last_sent = self._recent_tts_content[normalized_text]
            if current_time - last_sent < self._tts_dedup_window_seconds:
                self.ten_env.log_info(
                    f'[TTS_DEDUP] Skipping duplicate: "{text}" (sent {current_time - last_sent:.2f}s ago)'
                )
                return

        # Clean up old entries from tracking dict
        self._recent_tts_content = {
            k: v
            for k, v in self._recent_tts_content.items()
            if current_time - v < self._tts_dedup_window_seconds * 2
        }

        # Track this content
        self._recent_tts_content[normalized_text] = current_time

        # Log first TTS request for latency measurement
        if not hasattr(
            self, "_logged_tts_first_request"
        ) or self.turn_id != getattr(self, "_last_logged_tts_turn", -1):
            self.ten_env.log_info(
                f'[LATENCY_TTS_REQUEST] t={current_time:.3f} turn={self.turn_id} text="{text[:50]}"'
            )
            self._logged_tts_first_request = True
            self._last_logged_tts_turn = self.turn_id

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
        self._recent_tts_content.clear()  # Clear dedup caches on interrupt
        self._recent_transcript_content.clear()
        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        await _send_cmd(self.ten_env, "flush", "avatar")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")
