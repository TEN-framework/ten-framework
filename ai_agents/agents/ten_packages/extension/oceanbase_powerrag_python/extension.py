#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import time
import traceback
from dataclasses import dataclass
from typing import AsyncGenerator

import aiohttp
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
    VideoFrame,
)
from ten_ai_base.config import BaseConfig
from ten_ai_base.types import (
    LLMChatCompletionUserMessageParam,
    LLMDataCompletionArgs,
)
from ten_ai_base.llm import (
    AsyncLLMBaseExtension,
)

CMD_IN_FLUSH = "flush"
CMD_IN_ON_USER_JOINED = "on_user_joined"
CMD_IN_ON_USER_LEFT = "on_user_left"
CMD_OUT_FLUSH = "flush"
CMD_OUT_TOOL_CALL = "tool_call"

DATA_IN_TEXT_DATA_PROPERTY_IS_FINAL = "is_final"
DATA_IN_TEXT_DATA_PROPERTY_TEXT = "text"

DATA_OUT_TEXT_DATA_PROPERTY_TEXT = "text"
DATA_OUT_TEXT_DATA_PROPERTY_END_OF_SEGMENT = "end_of_segment"

CMD_PROPERTY_RESULT = "tool_result"


def is_punctuation(char):
    if char in [",", "，", ".", "。", "?", "？", "!", "！"]:
        return True
    return False


def parse_sentences(sentence_fragment, content):
    sentences = []
    current_sentence = sentence_fragment
    for char in content:
        current_sentence += char
        if is_punctuation(char):
            stripped_sentence = current_sentence
            if any(c.isalnum() for c in stripped_sentence):
                sentences.append(stripped_sentence)
            current_sentence = ""

    remain = current_sentence
    return sentences, remain


@dataclass
class OceanBasePowerRAGConfig(BaseConfig):
    base_url: str = ""
    api_key: str = ""
    ai_database_name: str = ""
    collection_id: str = ""
    user_id: str = "TenAgent"
    greeting: str = ""
    failure_info: str = ""


class OceanBasePowerRAGExtension(AsyncLLMBaseExtension):
    config: OceanBasePowerRAGConfig = None
    ten_env: AsyncTenEnv = None
    loop: asyncio.AbstractEventLoop = None
    stopped: bool = False
    users_count = 0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        await super().on_start(ten_env)
        ten_env.log_debug("on_start")
        self.loop = asyncio.get_event_loop()

        self.config = await OceanBasePowerRAGConfig.create_async(
            ten_env=ten_env
        )
        ten_env.log_info(f"config: {self.config}")

        if not self.config.base_url:
            ten_env.log_error("Missing required configuration: base_url")
            return

        if not self.config.api_key:
            ten_env.log_error("Missing required configuration: api_key")
            return

        if not self.config.ai_database_name:
            ten_env.log_error(
                "Missing required configuration: ai_database_name"
            )
            return

        if not self.config.collection_id:
            ten_env.log_error("Missing required configuration: collection_id")
            return

        self.ten_env = ten_env

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

        self.stopped = True

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        status = StatusCode.OK
        detail = "success"

        if cmd_name == CMD_IN_FLUSH:
            await self.flush_input_items(ten_env)
            await ten_env.send_cmd(Cmd.create(CMD_OUT_FLUSH))
            ten_env.log_info("on flush")
        elif cmd_name == CMD_IN_ON_USER_JOINED:
            self.users_count += 1
            # Send greeting when first user joined
            if self.config.greeting and self.users_count == 1:
                self.send_text_output(ten_env, self.config.greeting, True)
        elif cmd_name == CMD_IN_ON_USER_LEFT:
            self.users_count -= 1
        else:
            await super().on_cmd(ten_env, cmd)
            return

        cmd_result = CmdResult.create(status, cmd)
        cmd_result.set_property_string("detail", detail)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info("on_data name {}".format(data_name))

        is_final = False
        input_text = ""
        try:
            is_final, _ = data.get_property_bool(
                DATA_IN_TEXT_DATA_PROPERTY_IS_FINAL
            )
        except Exception as err:
            ten_env.log_info(
                f"GetProperty optional {DATA_IN_TEXT_DATA_PROPERTY_IS_FINAL} failed, err: {err}"
            )

        try:
            input_text, _ = data.get_property_string(
                DATA_IN_TEXT_DATA_PROPERTY_TEXT
            )
        except Exception as err:
            ten_env.log_info(
                f"GetProperty optional {DATA_IN_TEXT_DATA_PROPERTY_TEXT} failed, err: {err}"
            )

        if not is_final:
            ten_env.log_info("ignore non-final input")
            return
        if not input_text:
            ten_env.log_info("ignore empty text")
            return

        ten_env.log_info(f"OnData input text: [{input_text}]")

        # Start an asynchronous task for handling chat completion
        message = LLMChatCompletionUserMessageParam(
            role="user", content=input_text
        )
        await self.queue_input_item(False, messages=[message])

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        pass

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        pass

    async def on_call_chat_completion(self, async_ten_env, **kargs):
        raise NotImplementedError

    async def on_tools_update(self, async_ten_env, tool):
        raise NotImplementedError

    async def on_data_chat_completion(
        self, ten_env: AsyncTenEnv, **kargs: LLMDataCompletionArgs
    ) -> None:
        input_messages: LLMChatCompletionUserMessageParam = kargs.get(
            "messages", []
        )
        if not input_messages:
            ten_env.log_warn("No message in data")

        # Send fixed response first
        self.send_text_output(
            ten_env,
            sentence="PowerRAG is processing your request, please wait...",
            end_of_segment=True,
        )

        total_output = ""
        sentence_fragment = ""

        sentences = []
        self.ten_env.log_info(f"messages: {input_messages}")
        response = self._stream_chat(query=input_messages[0]["content"])
        async for message in response:
            self.ten_env.log_info(f"content: {message}")

            # Handle JSON-formatted streaming messages from OceanBase PowerRAG
            if isinstance(message, dict):
                content = message.get("content", "")
                if content:
                    total_output += content
                    sentences, sentence_fragment = parse_sentences(
                        sentence_fragment, content
                    )
                    for s in sentences:
                        await self._send_text(s, False)
                elif message.get("error"):
                    error_msg = message.get("error", "Unknown error occurred")
                    ten_env.log_error(f"error: {error_msg}")
                    await self._send_text(error_msg, True)
                    return
            else:
                # Handle plain text responses
                if message:
                    total_output += message
                    sentences, sentence_fragment = parse_sentences(
                        sentence_fragment, message
                    )
                    for s in sentences:
                        await self._send_text(s, False)

        await self._send_text(sentence_fragment, True)
        self.ten_env.log_info(f"total_output: {total_output}")

    async def _stream_chat(self, query: str) -> AsyncGenerator[dict, None]:
        async with aiohttp.ClientSession() as session:
            try:
                payload = {"stream": True, "jsonFormat": True, "content": query}

                self.ten_env.log_info(
                    f"payload before sending: {json.dumps(payload)}"
                )

                headers = {
                    "Authorization": self.config.api_key,
                    "Content-Type": "application/json",
                }

                url = f"{self.config.base_url}/{self.config.ai_database_name}/collections/{self.config.collection_id}/chat"
                self.ten_env.log_info(f"oceanbase powerRAG url: {url}")

                start_time = time.time()
                async with session.put(
                    url, json=payload, headers=headers
                ) as response:
                    if response.status != 200:
                        r = await response.text()
                        self.ten_env.log_error(
                            f"Received unexpected status {response.status}: {r} from the server."
                        )
                        if self.config.failure_info:
                            await self._send_text(
                                self.config.failure_info, True
                            )
                        return
                    end_time = time.time()
                    self.ten_env.log_info(
                        f"connect time {end_time - start_time} s"
                    )

                    async for line in response.content:
                        if not line:
                            continue
                        raw_line = line.decode("utf-8").strip()
                        if not raw_line:
                            continue

                        # Process only SSE lines starting with "data:"
                        if not raw_line.startswith("data:"):
                            continue

                        payload_str = raw_line[5:].strip()
                        if not payload_str:
                            continue

                        try:
                            payload_json = json.loads(raw_line[5:].strip())
                        except json.JSONDecodeError:
                            # Ignore non-JSON data lines
                            continue

                        answer = payload_json.get("answer")
                        if isinstance(answer, dict):
                            content_text = answer.get("content")
                            if content_text and content_text != "[DONE]":
                                yield content_text
            except Exception as e:
                traceback.print_exc()
                self.ten_env.log_error(f"Failed to handle {e}")
            finally:
                await session.close()
                session = None

    async def _send_text(self, text: str, end_of_segment: bool) -> None:
        data = Data.create("text_data")
        data.set_property_string(DATA_OUT_TEXT_DATA_PROPERTY_TEXT, text)
        data.set_property_bool(
            DATA_OUT_TEXT_DATA_PROPERTY_END_OF_SEGMENT, end_of_segment
        )
        asyncio.create_task(self.ten_env.send_data(data))
