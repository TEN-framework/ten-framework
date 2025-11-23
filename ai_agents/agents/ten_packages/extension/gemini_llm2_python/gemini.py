#
#
# Agora Real Time Engagement
# Google Gemini LLM2 Integration
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from dataclasses import dataclass, field
import json
from typing import AsyncGenerator, List
from pydantic import BaseModel
import httpx

from ten_ai_base.struct import (
    LLMMessageContent,
    LLMMessageFunctionCall,
    LLMMessageFunctionCallOutput,
    LLMRequest,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
    LLMResponseToolCall,
    TextContent,
)
from ten_ai_base.types import LLMToolMetadata
from ten_runtime.async_ten_env import AsyncTenEnv


@dataclass
class GeminiLLM2Config(BaseModel):
    api_key: str = ""
    model: str = "gemini-3-pro-preview"
    base_url: str = "https://generativelanguage.googleapis.com/v1beta/openai"
    temperature: float = 0.7
    top_p: float = 0.95
    max_tokens: int = 4096
    prompt: str = "You are a helpful assistant."
    black_list_params: List[str] = field(
        default_factory=lambda: ["messages", "tools", "stream", "model"]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class GeminiChatAPI:
    def __init__(self, ten_env: AsyncTenEnv, config: GeminiLLM2Config):
        self.config = config
        self.ten_env = ten_env
        ten_env.log_info(
            f"GeminiChatAPI initialized with model: {config.model} "
            f"(base_url={config.base_url})"
        )
        self.http_client = httpx.AsyncClient(
            base_url=self.config.base_url,
            timeout=30.0,
        )
        self._request_headers = {}
        self._request_params = {}
        if self.config.api_key:
            # OpenAI-compatible endpoint expects Bearer token in
            # Authorization header
            self._request_headers["Authorization"] = (
                f"Bearer {self.config.api_key}"
            )

    def _convert_tools_to_dict(self, tool: LLMToolMetadata):
        """Convert LLMToolMetadata to Gemini function definition format."""
        json_dict = {
            "type": "function",
            "function": {
                "name": tool.name,
                "description": tool.description,
                "parameters": {
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            },
        }

        for param in tool.parameters:
            properties = json_dict["function"]["parameters"]["properties"]
            properties[param.name] = {
                "type": param.type,
                "description": param.description,
            }
            if param.required:
                required = json_dict["function"]["parameters"]["required"]
                required.append(param.name)

        return json_dict

    def _parse_message_content(self, message: LLMMessageContent):
        """Parse message content into a plain text payload for Gemini."""
        content = message.content
        if isinstance(content, str):
            return content

        text_content = ""
        if isinstance(content, list):
            for part in content:
                if isinstance(part, TextContent):
                    text_content += part.text
                elif isinstance(part, dict):
                    if part.get("type") == "text":
                        text_content += part.get("text", "")
        return text_content

    async def get_chat_completions(
        self, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        """Stream chat completions from Gemini API."""
        try:
            # Build system prompt
            system_prompt = self.config.prompt

            # Convert messages
            parsed_messages = []
            tools = None

            for message in request_input.messages or []:
                if isinstance(message, LLMMessageContent):
                    role = message.role.lower()
                    content = self._parse_message_content(message)
                    if content:
                        parsed_messages.append(
                            {"role": role, "content": content}
                        )
                elif isinstance(message, LLMMessageFunctionCall):
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
                elif isinstance(message, LLMMessageFunctionCallOutput):
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

            # Build request - only include Gemini-compatible parameters
            req = {
                "model": self.config.model,
                "messages": [
                    {"role": "system", "content": system_prompt},
                    *parsed_messages,
                ],
                "temperature": self.config.temperature,
                "top_p": self.config.top_p,
                "max_tokens": self.config.max_tokens,
                "stream": request_input.streaming,
            }

            # Only add tools if present
            if tools:
                req["tools"] = tools

            # Add additional parameters if they are not in the black list
            for key, value in (request_input.parameters or {}).items():
                if not self.config.is_black_list_params(key):
                    self.ten_env.log_debug(f"set gemini param: {key} = {value}")
                    req[key] = value

            self.ten_env.log_info(
                f"Requesting chat completions with model: {self.config.model}"
            )

            # Make the API request
            response = await self.http_client.post(
                "/chat/completions",
                json=req,
                params=self._request_params,
                headers=self._request_headers,
            )

            if response.status_code != 200:
                error_text = response.text
                extra_hint = ""
                if response.status_code == 404:
                    extra_hint = (
                        " (check that the model exists at the configured "
                        "base_url; Vertex users may need a project-specific "
                        "endpoint)"
                    )
                error_msg = (
                    f"Gemini API error: {response.status_code} - "
                    f"{error_text}{extra_hint}"
                )
                self.ten_env.log_error(error_msg)
                raise RuntimeError(
                    f"Gemini API error: {response.status_code} - {error_text}"
                )

            # Handle streaming response
            if request_input.streaming:
                async for line in response.aiter_lines():
                    if not line or line.startswith("data: [DONE]"):
                        continue
                    if line.startswith("data: "):
                        try:
                            chunk_data = json.loads(line[6:])
                            if chunk_data.get("choices"):
                                choice = chunk_data["choices"][0]
                                delta = choice.get("delta", {})
                                if delta.get("content"):
                                    content = delta["content"]
                                    yield LLMResponseMessageDelta(
                                        response_id=chunk_data.get("id", ""),
                                        role="assistant",
                                        content=content,
                                        created=chunk_data.get("created", 0),
                                    )
                                # Check for tool calls
                                if delta.get("tool_calls"):
                                    for tool_call in delta["tool_calls"]:
                                        function = tool_call.get("function", {})
                                        yield LLMResponseToolCall(
                                            response_id=chunk_data.get(
                                                "id", ""
                                            ),
                                            call_id=tool_call.get("id", ""),
                                            name=function.get("name", ""),
                                            arguments=function.get(
                                                "arguments", ""
                                            ),
                                        )
                        except json.JSONDecodeError:
                            self.ten_env.log_debug(
                                f"Could not parse line: {line}"
                            )
                            continue

                # Send completion message
                yield LLMResponseMessageDone(
                    response_id="",
                    role="assistant",
                )
            else:
                # Non-streaming response
                response_data = response.json()
                if response_data.get("choices"):
                    choice = response_data["choices"][0]
                    content = choice.get("message", {}).get("content", "")
                    if content:
                        yield LLMResponseMessageDelta(
                            response_id=response_data.get("id", ""),
                            role="assistant",
                            content=content,
                            created=response_data.get("created", 0),
                        )
                    yield LLMResponseMessageDone(
                        response_id=response_data.get("id", ""),
                        role="assistant",
                    )

        except Exception as e:
            self.ten_env.log_error(f"Error in get_chat_completions: {e}")
            raise RuntimeError(f"CreateChatCompletion failed, err: {e}") from e
