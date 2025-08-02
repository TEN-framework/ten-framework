#
#
# Agora Real Time Engagement
# Created by Wei Hu in 2024-08.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from collections import defaultdict
from dataclasses import dataclass
from enum import Enum
import random
from typing import AsyncGenerator
from pydantic import BaseModel
import requests
from openai import AsyncOpenAI, AsyncStream
from openai.types.chat import ChatCompletionChunk

from ten_ai_base.struct import (
    LLMInput,
    LLMOutput,
    LLMOutputChoice,
    LLMOutputChoiceDelta,
)
from ten_runtime.async_ten_env import AsyncTenEnv


@dataclass
class OpenAILLM2Config(BaseModel):
    api_key: str = ""
    base_url: str = "https://api.openai.com/v1"
    model: str = (
        "gpt-4o"  # Adjust this to match the equivalent of `openai.GPT4o` in the Python library
    )
    proxy_url: str = ""
    temperature: float = 0.7
    top_p: float = 1.0
    presence_penalty: float = 0.0
    frequency_penalty: float = 0.0
    max_tokens: int = 4096
    seed: int = random.randint(0, 1000000)
    prompt: str = "You are a helpful assistant."


class ReasoningMode(str, Enum):
    ModeV1 = "v1"


class ThinkParser:
    def __init__(self):
        self.state = "NORMAL"  # States: 'NORMAL', 'THINK'
        self.think_content = ""
        self.content = ""

    def process(self, new_chars):
        if new_chars == "<think>":
            self.state = "THINK"
            return True
        elif new_chars == "</think>":
            self.state = "NORMAL"
            return True
        else:
            if self.state == "THINK":
                self.think_content += new_chars
        return False

    def process_by_reasoning_content(self, reasoning_content):
        state_changed = False
        if reasoning_content:
            if self.state == "NORMAL":
                self.state = "THINK"
                state_changed = True
            self.think_content += reasoning_content
        elif self.state == "THINK":
            self.state = "NORMAL"
            state_changed = True
        return state_changed


class OpenAIChatGPT:
    client = None

    def __init__(self, ten_env: AsyncTenEnv, config: OpenAILLM2Config):
        self.config = config
        self.ten_env = ten_env
        ten_env.log_info(
            f"OpenAIChatGPT initialized with config: {config.api_key}"
        )
        self.client = AsyncOpenAI(
            api_key=config.api_key,
            base_url=config.base_url,
            default_headers={
                "api-key": config.api_key,
                "Authorization": f"Bearer {config.api_key}",
            },
        )
        self.session = requests.Session()
        if config.proxy_url:
            proxies = {
                "http": config.proxy_url,
                "https": config.proxy_url,
            }
            ten_env.log_info(f"Setting proxies: {proxies}")
            self.session.proxies.update(proxies)
        self.client.session = self.session

    async def get_chat_completions(
        self, input: LLMInput
    ) -> AsyncGenerator[LLMOutput, None]:
        messages = input.messages
        tools = None
        parsed_messages = []

        for message in messages:
            if message.role == "user":
                parsed_messages.append(
                    {"role": "user", "content": message.content}
                )
            elif message.role == "assistant":
                parsed_messages.append(
                    {"role": "assistant", "content": message.content}
                )
            elif message.role == "system":
                parsed_messages.append(
                    {"role": "system", "content": message.content}
                )
            elif message.role == "tool":
                parsed_messages.append(
                    {
                        "role": "tool",
                        "content": message.content,
                        "tool_call_id": message.tool_call_id,
                    }
                )
            else:
                self.ten_env.log_warn(f"Unknown role: {message.role}")

        for tool in input.tools or []:
            if tools is None:
                tools = []
            tool_json = {
                "type": "function",
                "function": {
                    "name": tool.name,
                    "description": tool.description,
                    "parameters": [],
                },
            }
            for param in tool.parameters or []:
                tool_json["function"]["parameters"].append(
                    {
                        "name": param.name,
                        "type": param.type,
                        "description": param.description,
                        "required": param.required,
                    }
                )

        req = {
            "model": self.config.model,
            "messages": [
                {
                    "role": "system",
                    "content": self.config.prompt,
                },
                *messages,
            ],
            "tools": tools,
            "temperature": self.config.temperature,
            "top_p": self.config.top_p,
            "presence_penalty": self.config.presence_penalty,
            "frequency_penalty": self.config.frequency_penalty,
            "max_tokens": self.config.max_tokens,
            "seed": self.config.seed,
            "stream": input.streaming,
            "n": 1,  # Assuming single response for now
        }

        try:
            response: AsyncStream[ChatCompletionChunk] = (
                await self.client.chat.completions.create(**req)
            )

            full_content = ""
            # Check for tool calls
            tool_calls_dict = defaultdict(
                lambda: {
                    "id": None,
                    "function": {"arguments": "", "name": None},
                    "type": None,
                }
            )

            # Example usage
            parser = ThinkParser()
            reasoning_mode = None

            async for chat_completion in response:
                self.ten_env.log_info(f"Chat completion: {chat_completion}")
                if len(chat_completion.choices) == 0:
                    continue
                choice = chat_completion.choices[0]
                delta = choice.delta

                llm_choice = LLMOutputChoice(
                    finish_reason=choice.finish_reason,
                    index=choice.index,
                    logprobs=None,  # Assuming no logprobs for simplicity
                    delta=LLMOutputChoiceDelta(
                        content=(
                            delta.content if delta and delta.content else ""
                        ),
                        role=delta.role,
                        refusal=delta.refusal,
                        tool_calls=[],
                    ),
                )

                yield LLMOutput(
                    id=chat_completion.id,
                    choice=llm_choice,
                    created=chat_completion.created,
                    model=chat_completion.model,
                )

                # content = delta.content if delta and delta.content else ""
                # reasoning_content = (
                #     delta.reasoning_content
                #     if delta
                #     and hasattr(delta, "reasoning_content")
                #     and delta.reasoning_content
                #     else ""
                # )

                # if reasoning_mode is None and reasoning_content is not None:
                #     reasoning_mode = ReasoningMode.ModeV1

                # Emit content update event (fire-and-forget)
            #     if listener and (content or reasoning_mode == ReasoningMode.ModeV1):
            #         prev_state = parser.state

            #         if reasoning_mode == ReasoningMode.ModeV1:
            #             self.ten_env.log_info("process_by_reasoning_content")
            #             think_state_changed = parser.process_by_reasoning_content(
            #                 reasoning_content
            #             )
            #         else:
            #             think_state_changed = parser.process(content)

            #         if not think_state_changed:
            #             # self.ten_env.log_info(f"state: {parser.state}, content: {content}, think: {parser.think_content}")
            #             if parser.state == "THINK":
            #                 listener.emit("reasoning_update", parser.think_content)
            #             elif parser.state == "NORMAL":
            #                 listener.emit("content_update", content)

            #         if prev_state == "THINK" and parser.state == "NORMAL":
            #             listener.emit(
            #                 "reasoning_update_finish", parser.think_content
            #             )
            #             parser.think_content = ""

            #     full_content += content

            #     if delta.tool_calls:
            #         try:
            #             for tool_call in delta.tool_calls:
            #                 self.ten_env.log_info(f"Tool call: {tool_call}")
            #                 if tool_call.index not in tool_calls_dict:
            #                     tool_calls_dict[tool_call.index] = {
            #                         "id": None,
            #                         "function": {"arguments": "", "name": None},
            #                         "type": None,
            #                     }

            #                 if tool_call.id:
            #                     tool_calls_dict[tool_call.index][
            #                         "id"
            #                     ] = tool_call.id

            #                 # If the function name is not None, set it
            #                 if tool_call.function.name:
            #                     tool_calls_dict[tool_call.index]["function"][
            #                         "name"
            #                     ] = tool_call.function.name

            #                 # Append the arguments if not None
            #                 if tool_call.function.arguments:
            #                     tool_calls_dict[tool_call.index]["function"][
            #                         "arguments"
            #                     ] += tool_call.function.arguments

            #                 # If the type is not None, set it
            #                 if tool_call.type:
            #                     tool_calls_dict[tool_call.index][
            #                         "type"
            #                     ] = tool_call.type
            #         except Exception as e:
            #             import traceback

            #             traceback.print_exc()
            #             self.ten_env.log_error(
            #                 f"Error processing tool call: {e} {tool_calls_dict}"
            #             )

            # # Convert the dictionary to a list
            # tool_calls_list = list(tool_calls_dict.values())

            # # Emit tool calls event (fire-and-forget)
            # if listener and tool_calls_list:
            #     for tool_call in tool_calls_list:
            #         listener.emit("tool_call", tool_call)

            # # Emit content finished event after the loop completes
            # if listener:
            #     listener.emit("content_finished", full_content)
        except Exception as e:
            raise RuntimeError(f"CreateChatCompletion failed, err: {e}") from e
