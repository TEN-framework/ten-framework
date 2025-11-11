#
#
# Agora Real Time Engagement
# Created by Wei Hu in 2024-08.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from collections import defaultdict
from dataclasses import dataclass, field
from enum import Enum
import json
import random
from typing import AsyncGenerator, List
from pydantic import BaseModel
import requests
from openai import AsyncOpenAI, AsyncStream
from openai.types.chat import ChatCompletionChunk

from ten_ai_base.struct import (
    ImageContent,
    LLMMessageContent,
    LLMMessageFunctionCall,
    LLMMessageFunctionCallOutput,
    LLMRequest,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
    LLMResponseReasoningDelta,
    LLMResponseReasoningDone,
    LLMResponseToolCall,
    TextContent,
)
from ten_ai_base.types import LLMToolMetadata
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
    black_list_params: List[str] = field(
        default_factory=lambda: ["messages", "tools", "stream", "n", "model"]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class ReasoningMode(str, Enum):
    ModeV1 = "v1"


class ThinkParser:
    def __init__(self):
        self.state = "NORMAL"  # States: 'NORMAL', 'THINK'
        self.think_content = ""
        self.content = ""
        self.think_delta = ""

    def process(self, new_chars):
        if new_chars == "<think>":
            self.state = "THINK"
            self.think_delta = ""
            return True
        elif new_chars == "</think>":
            self.state = "NORMAL"
            self.think_delta = ""
            return True
        else:
            if self.state == "THINK":
                self.think_content += new_chars
                self.think_delta = new_chars
        return False

    def process_by_reasoning_content(self, reasoning_content):
        state_changed = False
        if reasoning_content:
            if self.state == "NORMAL":
                self.state = "THINK"
                state_changed = True
            self.think_content += reasoning_content
            self.think_delta = reasoning_content
        elif self.state == "THINK":
            self.state = "NORMAL"
            self.think_delta = ""
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

    def _convert_tools_to_dict(self, tool: LLMToolMetadata):
        json_dict = {
            "type": "function",
            "function": {
                "name": tool.name,
                "description": tool.description,
                "parameters": {
                    "type": "object",
                    "properties": {},
                    "required": [],
                    "additionalProperties": False,
                },
            },
            "strict": True,
        }

        for param in tool.parameters:
            json_dict["function"]["parameters"]["properties"][param.name] = {
                "type": param.type,
                "description": param.description,
            }
            if param.required:
                json_dict["function"]["parameters"]["required"].append(
                    param.name
                )
            if param.type == "array":
                json_dict["function"]["parameters"]["properties"][param.name][
                    "items"
                ] = param.items

        return json_dict

    async def get_chat_completions(
        self, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        messages = request_input.messages
        tools = None
        parsed_messages = []
        system_prompt = request_input.prompt or self.config.prompt

        self.ten_env.log_info(
            f"get_chat_completions: {len(messages)} messages, streaming: {request_input.streaming}"
        )

        for message in messages:
            match message:
                case LLMMessageContent():
                    role = message.role
                    content = message.content
                    if isinstance(content, str):
                        parsed_messages.append(
                            {"role": role, "content": content}
                        )
                    elif isinstance(content, list):
                        # Assuming content is a list of objects
                        content_items = []
                        for item in content:
                            match item:
                                case TextContent():
                                    content_items.append(
                                        {"type": "text", "text": item.text}
                                    )
                                case ImageContent():
                                    content_items.append(
                                        {
                                            "type": "image_url",
                                            "image_url": {
                                                "url": item.image_url.url
                                            },
                                        }
                                    )
                        parsed_messages.append(
                            {"role": role, "content": content_items}
                        )
                case LLMMessageFunctionCall():
                    # Handle function call messages
                    parsed_messages.append(
                        {
                            "role": "assistant",
                            "tool_calls": [
                                {
                                    "id": message.call_id,
                                    "type": "function",
                                    "function": {
                                        "name": message.name,
                                        "arguments": message.arguments,
                                    },
                                }
                            ],
                        }
                    )
                case LLMMessageFunctionCallOutput():
                    # Handle function call output messages
                    parsed_messages.append(
                        {
                            "role": "tool",
                            "tool_call_id": message.call_id,
                            "content": message.output,
                        }
                    )

        for tool in request_input.tools or []:
            if tools is None:
                tools = []
            tools.append(self._convert_tools_to_dict(tool))

        req = {
            "model": self.config.model,
            "messages": [
                {"role": "system", "content": system_prompt},
                *parsed_messages,
            ],
            "tools": tools,
            "temperature": self.config.temperature,
            "top_p": self.config.top_p,
            "presence_penalty": self.config.presence_penalty,
            "frequency_penalty": self.config.frequency_penalty,
            "max_tokens": self.config.max_tokens,
            "seed": self.config.seed,
            "stream": request_input.streaming,
            "n": 1,  # Assuming single response for now
        }

        # Add additional parameters if they are not in the black list
        for key, value in (request_input.parameters or {}).items():
            # Check if it's a valid option and not in black list
            if not self.config.is_black_list_params(key):
                self.ten_env.log_debug(f"set openai param: {key} = {value}")
                req[key] = value

        self.ten_env.log_info(f"Requesting chat completions with: {req}")

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

            last_chat_completion: ChatCompletionChunk | None = None

            async for chat_completion in response:
                self.ten_env.log_debug(f"Chat completion: {chat_completion}")
                if chat_completion is None or len(chat_completion.choices) == 0:
                    continue
                last_chat_completion = chat_completion
                choice = chat_completion.choices[0]
                delta = choice.delta

                self.ten_env.log_debug(f"Processing choice: {choice}")

                content = delta.content if delta and delta.content else ""
                reasoning_content = (
                    delta.reasoning_content
                    if delta
                    and hasattr(delta, "reasoning_content")
                    and delta.reasoning_content
                    else ""
                )

                if reasoning_mode is None and reasoning_content is not None:
                    reasoning_mode = ReasoningMode.ModeV1

                # Emit content update event (fire-and-forget)
                if content or reasoning_mode == ReasoningMode.ModeV1:
                    prev_state = parser.state

                    if reasoning_mode == ReasoningMode.ModeV1:
                        self.ten_env.log_debug("process_by_reasoning_content")
                        think_state_changed = (
                            parser.process_by_reasoning_content(
                                reasoning_content
                            )
                        )
                    else:
                        think_state_changed = parser.process(content)

                    if not think_state_changed:
                        self.ten_env.log_debug(
                            f"state: {parser.state}, content: {content}, think: {parser.think_content}"
                        )
                        if parser.state == "THINK":
                            yield LLMResponseReasoningDelta(
                                response_id=chat_completion.id,
                                role="assistant",
                                content=parser.think_content,
                                delta=parser.think_delta,
                                created=chat_completion.created,
                            )
                        elif parser.state == "NORMAL":
                            yield LLMResponseMessageDelta(
                                response_id=chat_completion.id,
                                role="assistant",
                                content=full_content + content,
                                delta=content,
                                created=chat_completion.created,
                            )

                    if prev_state == "THINK" and parser.state == "NORMAL":
                        yield LLMResponseReasoningDone(
                            response_id=chat_completion.id,
                            role="assistant",
                            content=parser.think_content,
                            created=chat_completion.created,
                        )
                        parser.think_content = ""

                full_content += content

                if delta.tool_calls:
                    try:
                        for tool_call in delta.tool_calls:
                            self.ten_env.log_info(f"Tool call: {tool_call}")
                            if tool_call.index not in tool_calls_dict:
                                tool_calls_dict[tool_call.index] = {
                                    "id": None,
                                    "function": {"arguments": "", "name": None},
                                    "type": None,
                                }

                            if tool_call.id:
                                tool_calls_dict[tool_call.index][
                                    "id"
                                ] = tool_call.id

                            # If the function name is not None, set it
                            if tool_call.function.name:
                                tool_calls_dict[tool_call.index]["function"][
                                    "name"
                                ] = tool_call.function.name

                            # Append the arguments if not None
                            if tool_call.function.arguments:
                                tool_calls_dict[tool_call.index]["function"][
                                    "arguments"
                                ] += tool_call.function.arguments

                            # If the type is not None, set it
                            if tool_call.type:
                                tool_calls_dict[tool_call.index][
                                    "type"
                                ] = tool_call.type
                    except Exception as e:
                        import traceback

                        traceback.print_exc()
                        self.ten_env.log_error(
                            f"Error processing tool call: {e} {tool_calls_dict}"
                        )

            if last_chat_completion is None:
                self.ten_env.log_info("No chat completion choices found.")
                return

            # Convert the dictionary to a list
            tool_calls_list = list(tool_calls_dict.values())

            # Emit tool calls event (fire-and-forget)
            if tool_calls_list:
                for tool_call in tool_calls_list:
                    arguements = json.loads(tool_call["function"]["arguments"])
                    self.ten_env.log_info(
                        f"Tool call22: {choice.delta.model_dump_json()}"
                    )
                    yield LLMResponseToolCall(
                        response_id=last_chat_completion.id,
                        id=last_chat_completion.id,
                        tool_call_id=tool_call["id"],
                        name=tool_call["function"]["name"],
                        arguments=arguements,
                        created=last_chat_completion.created,
                    )

            # Emit content finished event after the loop completes
            yield LLMResponseMessageDone(
                response_id=last_chat_completion.id,
                role="assistant",
                content=full_content,
                created=last_chat_completion.created,
            )
        except Exception as e:
            raise RuntimeError(f"CreateChatCompletion failed, err: {e}") from e
