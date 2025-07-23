#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from enum import Enum
import json
import time
from typing import Any, Awaitable, Callable
import uuid
from pydantic import BaseModel
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)

class MainControlConfig(BaseModel):
    greeting: str = "Hello there, I'm TEN Agent"

class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self._rtc_user_count = 0
        self.config = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd name {cmd_name}")

        if cmd_name == "on_user_joined":
            await self._on_cmd_on_user_joined(ten_env, cmd)
        elif cmd_name == "on_user_left":
            await self._on_cmd_on_user_left(ten_env, cmd)

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def _on_cmd_on_user_joined(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        assert self.config is not None
        self._rtc_user_count += 1
        if self._rtc_user_count == 1 and self.config.greeting:
            await self._request_tts(ten_env, self.config.greeting)
            await self._send_caption(ten_env, self.config.greeting, True, True, 100)

    async def _on_cmd_on_user_left(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        self._rtc_user_count -= 1

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data name {data_name}")

        if data_name == "asr_result":
            await self._on_data_asr_result(ten_env, data)
        elif data_name == "llm_result":
            await self._on_data_llm_result(ten_env, data)

    async def _on_data_asr_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        asr_result_json, _ = data.get_property_to_json(None)
        asr_result_dict = json.loads(asr_result_json)
        text = asr_result_dict.get("text", "")
        final = asr_result_dict.get("final", False)
        metadata = asr_result_dict.get("metadata", {})
        stream_id = int(metadata.get("session_id", "100"))
        if final or len(text) > 2:
            await self._flush(ten_env)
        if final:
            await self._request_llm(ten_env, text, True)

        await self._send_caption(ten_env, text, final, final, stream_id)

    async def _on_data_llm_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        llm_result_json, _ = data.get_property_to_json(None)
        llm_result_dict = json.loads(llm_result_json)
        text = llm_result_dict.get("text", "")
        end_of_segment = llm_result_dict.get("end_of_segment", False)
        await self._request_tts(ten_env, text)
        await self._send_caption(ten_env, text, end_of_segment, True, 100)

    async def _request_tts(self, ten_env: AsyncTenEnv, text: str):
        q = Data.create("text_data")
        q.set_dest(None, None, "tts")
        q.set_property_string("text", text)
        await ten_env.send_data(q)
        ten_env.log_info(f"request_tts: text {text}")

    async def _request_llm(self, ten_env: AsyncTenEnv, text: str, is_final: bool):
        q = Data.create("text_data")
        q.set_dest(None, None, "llm")
        q.set_property_string("text", text)
        q.set_property_bool("is_final", is_final)
        await ten_env.send_data(q)
        ten_env.log_info(f"request_llm: text {text}, is_final {is_final}")

    async def _send_caption(self, ten_env: AsyncTenEnv, text: str, end_of_segment: bool, final: bool, stream_id: int):
        caption = Data.create("text_data")
        caption.set_dest(None, None, "message_collector")
        caption.set_property_string("text", text)
        caption.set_property_bool("is_final", final)
        caption.set_property_int("stream_id", stream_id)
        caption.set_property_bool("end_of_segment", end_of_segment)
        await ten_env.send_data(caption)
        ten_env.log_info(f"caption: text {text}, is_final {final}, end_of_segment {end_of_segment}, stream_id {stream_id}")

    async def _flush(self, ten_env: AsyncTenEnv):
        flush_llm = Cmd.create("flush")
        flush_llm.set_dest(None, None, "llm")
        await ten_env.send_cmd(flush_llm)
        ten_env.log_info("flush_llm")

        flush_tts = Cmd.create("flush")
        flush_tts.set_dest(None, None, "tts")
        await ten_env.send_cmd(flush_tts)
        ten_env.log_info("flush_tts")

        flush_rtc = Cmd.create("flush")
        flush_rtc.set_dest(None, None, "rtc")
        await ten_env.send_cmd(flush_rtc)
        ten_env.log_info("flush_rtc")