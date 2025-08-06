#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import time
import traceback
from typing import Literal, Tuple
from pydantic import BaseModel

from ten_ai_base.const import CMD_PROPERTY_TOOL
from ten_ai_base.types import LLMToolMetadata
from .llm_exec import LLMExec
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from .helper import _send_data, _send_cmd


class MainControlConfig(BaseModel):
    greeting: str = "Hello there, I'm TEN Agent"


class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self._rtc_user_count = 0
        self.config = None
        self.cmd_events = asyncio.Queue[Tuple[str, Cmd]]()
        self.data_events = asyncio.Queue[Tuple[str, Data]]()
        self.llm_exec: LLMExec = None
        self.ten_env: AsyncTenEnv = None
        self.stopped = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env

        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)
        ten_env.log_debug("on_init")

        self.llm_exec = LLMExec(ten_env)
        self.llm_exec.on_response = self._on_llm_response

        asyncio.create_task(self._process_cmd_events())
        asyncio.create_task(self._process_data_events())

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
        self.llm_exec.stopped = True
        self.stopped = True

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd name {cmd_name}")

        await self.cmd_events.put([cmd_name, cmd])

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data name {data_name}")

        await self.data_events.put([data_name, data])

    async def _process_data_events(self) -> None:
        while self.stopped is False:
            try:
                data_name, data = await self.data_events.get()
                match data_name:
                    case "asr_result":
                        asr_result_json, _ = data.get_property_to_json(None)
                        asr_result_dict = json.loads(asr_result_json)
                        text = asr_result_dict.get("text", "")
                        final = asr_result_dict.get("final", False)
                        metadata = asr_result_dict.get("metadata", {})
                        stream_id = int(metadata.get("session_id", "100"))
                        if final or len(text) > 2:
                            await self._interrupt(self.ten_env)
                        if final:
                            await self.llm_exec.queue_input(text)

                        await self._send_transcript(
                            self.ten_env, "user", text, final, stream_id
                        )
                    case _:
                        self.ten_env.log_info(f"Unknown data: {data_name}")
            except Exception as e:
                self.ten_env.log_error(f"Error processing data: {e}")

    async def _process_cmd_events(self) -> None:
        while self.stopped is False:
            try:
                cmd_name, cmd = await self.cmd_events.get()
                self.ten_env.log_info(f"Processing command: {cmd_name}")

                match cmd_name:
                    case "on_user_joined":
                        self._rtc_user_count += 1
                        if self._rtc_user_count == 1 and self.config.greeting:
                            await self._send_to_tts(
                                self.ten_env, self.config.greeting
                            )
                            await self._send_transcript(
                                self.ten_env,
                                "assistant",
                                self.config.greeting,
                                True,
                                100,  # Assuming a default stream ID
                            )
                    case "on_user_left":
                        self._rtc_user_count -= 1
                    case "tool_register":
                            tool_metadata_json, err = cmd.get_property_to_json(
                                CMD_PROPERTY_TOOL
                            )
                            if err:
                                raise RuntimeError(f"Failed to  get tool metadata: {err}")
                            self.ten_env.log_info(f"register tool: {tool_metadata_json}")
                            tool_metadata = LLMToolMetadata.model_validate_json(
                                tool_metadata_json
                            )
                            await self.llm_exec.register_tool(tool_metadata, cmd.get_source())
                            await self.ten_env.return_result(
                                CmdResult.create(StatusCode.OK, cmd)
                            )
                    case _:
                        self.ten_env.log_info(f"Unknown command: {cmd_name}")
            except Exception:
                self.ten_env.log_warn(
                    f"on_cmd failed: {traceback.format_exc()}"
                )
                await self.ten_env.return_result(
                    CmdResult.create(StatusCode.ERROR, cmd)
                )

    async def _send_to_tts(self, ten_env: AsyncTenEnv, text: str):
        await _send_data(ten_env, "text_data", "tts", {"text": text})
        ten_env.log_info(f"_send_to_tts: text {text}")

    async def _on_llm_response(
        self, ten_env: AsyncTenEnv, text: str, is_final: bool
    ):
        ten_env.log_info(f"_on_llm_response: text {text}, is_final {is_final}")
        if not is_final:
            await self._send_to_tts(ten_env, text)
        await self._send_transcript(ten_env, "assistant", text, is_final, 100)  # Assuming a default stream ID

    async def _send_transcript(
        self,
        ten_env: AsyncTenEnv,
        role: Literal["user", "assistant"],
        text: str,
        final: bool,
        stream_id: int,
    ):
        await _send_data(
            ten_env,
            "message",
            "message_collector",
            {
                "data_type": "transcribe",
                "role": role,
                "text": text,
                "text_ts": int(time.time() * 1000),  # Convert to milliseconds
                "is_final": final,
                "stream_id": stream_id,
            },
        )
        ten_env.log_info(
            f"_send_transcript: role {role}, text {text}, is_final {final}, stream_id {stream_id}"
        )


    async def _interrupt(self, ten_env: AsyncTenEnv):
        await self.llm_exec.flush()
        await _send_cmd(ten_env, "flush", "tts")
        await _send_cmd(ten_env, "flush", "agora_rtc")
