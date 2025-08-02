#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from typing import Any, AsyncGenerator, Awaitable, Callable, Optional, Tuple
from pydantic import BaseModel
from ten_ai_base.struct import LLMOutput
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Loc,
    StatusCode,
    CmdResult,
    Data,
    TenError,
)
from .helper import parse_sentences

class MainControlConfig(BaseModel):
    greeting: str = "Hello there, I'm TEN Agent"

class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self._rtc_user_count = 0
        self.config = None
        self.cmd_events = asyncio.Queue[Tuple[str, Cmd]]()
        self.data_events = asyncio.Queue[Tuple[str, Data]]()
        self.ten_env: AsyncTenEnv = None
        self.stopped = False
        self.sentence_fragment = ""

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)
        ten_env.log_debug("on_init")

        asyncio.create_task(self._process_cmd_events())
        asyncio.create_task(self._process_data_events())

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
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
                            asyncio.create_task(self._send_to_llm(self.ten_env, text, final, self._handle_llm_response))

                        await self._send_transcript(self.ten_env, text, final, final, stream_id)
                    case _:
                        self.ten_env.log_info(f"Unknown data: {data_name}")
            except Exception as e:
                self.ten_env.log_error(f"Error processing data: {e}")

    async def _process_cmd_events(self) -> None:
        while self.stopped is False:
            try:
                cmd_name, cmd = await self.cmd_events.get()

                match cmd_name:
                    case "on_user_joined":
                        self._rtc_user_count += 1
                        if self._rtc_user_count == 1 and self.config.greeting:
                            await self._send_to_tts(self.ten_env, self.config.greeting)
                            await self._send_transcript(self.ten_env, self.config.greeting, True, True, 100)
                    case "on_user_left":
                        self._rtc_user_count -= 1
                    case _:
                        self.ten_env.log_info(f"Unknown command: {cmd_name}")
            except Exception as e:
                self.ten_env.log_error(f"Error processing command: {e}")


    async def _send_to_tts(self, ten_env: AsyncTenEnv, text: str):
        await self._send_data(ten_env, "text_data", "tts", {"text": text})
        ten_env.log_info(f"_send_to_tts: text {text}")

    async def _send_to_llm(self, ten_env: AsyncTenEnv, text: str, is_final: bool, on_response: Optional[Callable[[LLMOutput], Awaitable[None]]] = None) -> None:
        response = self._send_cmd_ex(ten_env, "chat_completion", "llm", {
            "messages": [{"role": "user", "content": text}],
            "streaming": True,
            "tools": [],
            "model": "qwen-max",
            "parameters": {"max_tokens": 1000, "temperature": 0.7, "top_p": 1.0, "frequency_penalty": 0.0, "presence_penalty": 0.0, "stop_sequences": []}
        })

        async for cmd_result, _ in response:
            if cmd_result and cmd_result.is_final() is False:
                ten_env.log_info(f"_send_to_llm: cmd_result {cmd_result}")
                if cmd_result.get_status_code() == StatusCode.OK:
                    response_json, _ = cmd_result.get_property_to_json(None)
                    ten_env.log_debug(f"_send_to_llm: response_json {response_json}")
                    completion = LLMOutput.model_validate_json(response_json)
                    if on_response:
                        await on_response(completion)
        if on_response:
            await on_response(None, True)

    async def _handle_llm_response(self, llm_output: LLMOutput | None, is_final: bool = False):
        self.ten_env.log_info(f"_handle_llm_response: {llm_output}")

        if is_final:
            await self._send_transcript(self.ten_env, "", True, True, 100)
        else:
            text = llm_output.choice.delta.content
            if text:
                sentences, self.sentence_fragment = parse_sentences(
                    self.sentence_fragment, text
                )
                for sentence in sentences:
                    await self._send_to_tts(self.ten_env, sentence)
                    await self._send_transcript(
                        self.ten_env, sentence, is_final, True, 100)

    async def _send_transcript(self, ten_env: AsyncTenEnv, text: str, end_of_segment: bool, final: bool, stream_id: int):
        await self._send_data(ten_env, "text_data", "message_collector", {"text": text, "is_final": final, "stream_id": stream_id, "end_of_segment": end_of_segment})
        ten_env.log_info(f"_send_transcript: text {text}, is_final {final}, end_of_segment {end_of_segment}, stream_id {stream_id}")

    async def _interrupt(self, ten_env: AsyncTenEnv):
        await self._send_cmd(ten_env, "flush", "llm")
        await self._send_cmd(ten_env, "flush", "tts")
        await self._send_cmd(ten_env, "flush", "rtc")

    async def _send_cmd(
        self, ten_env: AsyncTenEnv, cmd_name: str, dest: str, payload: Any = None
    ) -> tuple[Optional[CmdResult], Optional[TenError]]:
        """Convenient method to send a command with a payload within app/graph w/o need to create a connection."""
        """Note: extension using this approach will contain logics that are meaningful for this graph only,"""
        """as it will assume the target extension already exists in the graph."""
        """For generate purpose extension, it should try to prevent using this method."""
        cmd = Cmd.create(cmd_name)
        loc = Loc("", "", dest)
        cmd.set_dests([loc])
        if payload is not None:
            cmd.set_property_from_json(None, json.dumps(payload))
        ten_env.log_debug(f"send_cmd: cmd_name {cmd_name}, dest {dest}")

        return await ten_env.send_cmd(cmd)

    async def _send_cmd_ex(
        self, ten_env: AsyncTenEnv, cmd_name: str, dest: str, payload: Any = None
    ) -> AsyncGenerator[tuple[Optional[CmdResult], Optional[TenError]], None]:
        """Convenient method to send a command with a payload within app/graph w/o need to create a connection."""
        """Note: extension using this approach will contain logics that are meaningful for this graph only,"""
        """as it will assume the target extension already exists in the graph."""
        """For generate purpose extension, it should try to prevent using this method."""
        cmd = Cmd.create(cmd_name)
        loc = Loc("", "", dest)
        cmd.set_dests([loc])
        if payload is not None:
            cmd.set_property_from_json(None, json.dumps(payload))
        ten_env.log_debug(f"send_cmd_ex: cmd_name {cmd_name}, dest {dest}")

        async for cmd_result, ten_error in ten_env.send_cmd_ex(cmd):
            if cmd_result:
                ten_env.log_debug(f"send_cmd_ex: cmd_result {cmd_result}")
                yield cmd_result, ten_error

    async def _send_data(
        self, ten_env: AsyncTenEnv, data_name: str, dest: str, payload: Any = None
    ) -> Optional[TenError]:
        """Convenient method to send data with a payload within app/graph w/o need to create a connection."""
        """Note: extension using this approach will contain logics that are meaningful for this graph only,"""
        """as it will assume the target extension already exists in the graph."""
        """For generate purpose extension, it should try to prevent using this method."""
        data = Data.create(data_name)
        loc = Loc("", "", dest)
        data.set_dests([loc])
        if payload is not None:
            data.set_property_from_json(None, json.dumps(payload))
        ten_env.log_debug(f"send_data: data_name {data_name}, dest {dest}")
        return await ten_env.send_data(data)