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
    Loc,
    StatusCode,
)
from ten_ai_base.const import CMD_PROPERTY_RESULT
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResultLLMResult,
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
        self._openclaw_tool_registered: bool = False
        self._tts_request_id: str | None = None

        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"

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
        if event.final or len(event.text) > 2:
            await self._interrupt()
        if event.final:
            self.turn_id += 1
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

        await self._send_transcript(
            "assistant",
            event.text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")
        await self._register_openclaw_tool()

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        if cmd.get_name() == "tool_call":
            handled = await self._handle_tool_call(cmd)
            if handled:
                return
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
        if data_type not in ("text", "reasoning"):
            return
        payload = {
            "type": "transcribe",
            "text": text,
            "is_final": final,
            "ts": int(time.time() * 1000),
            "stream_id": "0" if role == "assistant" else str(stream_id),
        }
        await self._send_rtm_message(payload)
        self.ten_env.log_info(
            f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
        )

    async def _send_rtm_message(self, payload: dict) -> None:
        message = json.dumps(payload)
        cmd = Cmd.create("publish")
        cmd.set_dests([Loc("", "", "agora_rtm")])
        cmd.set_property_buf("message", message.encode())
        await self.ten_env.send_cmd(cmd)

    async def _send_to_tts(self, text: str, is_final: bool):
        """
        Sends a sentence to the TTS system.
        """
        if self._tts_request_id is None:
            self._tts_request_id = f"tts-request-{uuid.uuid4()}"
        request_id = self._tts_request_id
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
        if is_final:
            self._tts_request_id = None
        self.ten_env.log_info(
            f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}"
        )

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        self.sentence_fragment = ""
        self._tts_request_id = None
        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")


    async def _register_openclaw_tool(self):
        if self._openclaw_tool_registered:
            return
        tool = LLMToolMetadata(
            name="claw_task_delegate",
            description="Delegate complex tasks to OpenClaw. Provide a short summary of the task.",
            parameters=[
                LLMToolMetadataParameter(
                    name="summary",
                    type="string",
                    description="Summary of the complex task to delegate.",
                    required=True,
                ),
            ],
        )
        await self.agent.register_llm_tool(tool, self.name)
        self._openclaw_tool_registered = True

    async def _handle_tool_call(self, cmd: Cmd) -> bool:
        try:
            name = ""
            args: dict = {}
            used_root_payload = False
            name, err = cmd.get_property_string("name")
            if err:
                payload_json, payload_err = cmd.get_property_to_json(None)
                if payload_err:
                    raise RuntimeError(
                        f"Failed to get tool call payload: {payload_err}"
                    )
                payload = json.loads(payload_json)
                name = str(payload.get("name", "")).strip()
                raw_args = payload.get("arguments", {})
                if isinstance(raw_args, str):
                    args = json.loads(raw_args)
                elif isinstance(raw_args, dict):
                    args = raw_args
                else:
                    args = {}
                used_root_payload = True
            if name != "claw_task_delegate":
                return False
            if not used_root_payload:
                args_json, err = cmd.get_property_to_json("arguments")
                if err:
                    raise RuntimeError(f"Failed to get tool arguments: {err}")
                args = json.loads(args_json)
            summary = str(args.get("summary", "")).strip()
            if summary:
                payload = {
                    "data_type": "openclaw_tool",
                    "summary": summary,
                }
                await self._send_rtm_message(payload)
            result = CmdResult.create(StatusCode.OK, cmd)
            tool_result = LLMToolResultLLMResult(
                type="llmresult",
                content="Sent to OpenClaw",
            )
            result.set_property_from_json(
                CMD_PROPERTY_RESULT, json.dumps(tool_result)
            )
            await self.ten_env.return_result(result)
            return True
        except Exception as e:
            self.ten_env.log_error(f"handle_tool_call error: {e}")
            await self.ten_env.return_result(
                CmdResult.create(StatusCode.ERROR, cmd)
            )
            return True
