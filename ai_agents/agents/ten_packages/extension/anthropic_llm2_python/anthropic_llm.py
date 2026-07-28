#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
import re
import time
from typing import Any, AsyncGenerator, Literal

import httpx
from anthropic import AsyncAnthropic
from pydantic import BaseModel, Field

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

# Server-side refusal fallback. This beta flag pairs with the scalar
# `fallbacks: "default"` form; the older array form uses a different flag and
# mixing the two returns a 400.
FALLBACK_BETA = "server-side-fallback-2026-07-01"

# Passed explicitly whenever `base_url` is unset. Leaving it to the SDK means
# it falls back to the ANTHROPIC_BASE_URL environment variable, and a variable
# that is exported but empty is not absent -- it becomes a base URL of "" and
# fails every request with a bare "Connection error". `property.json` maps the
# property to `${env:ANTHROPIC_BASE_URL|}`, so any deployment that exports the
# name without a value lands there.
DEFAULT_BASE_URL = "https://api.anthropic.com"

# data:image/png;base64,iVBOR...
DATA_URI_RE = re.compile(
    r"^data:(?P<media_type>[^;,]+);base64,(?P<data>.*)$", re.DOTALL
)

# Adaptive thinking, `output_config.effort`, and server-side refusal fallback
# only exist on the current generation. Sending them to one of the older models
# Anthropic still serves returns a 400 -- notably `effort` on Claude Haiku 4.5,
# which is otherwise a natural pick for latency-sensitive voice graphs.
#
# The list is intentionally a closed set of older served models rather than an
# allow-list of new ones, so a model released after this extension was written
# gets the current-generation path by default and stays correct. Each entry is
# also the prefix of that model's dated id, so `claude-haiku-4-5-20251001` and
# the `claude-haiku-4-5` alias both match. Claude Opus 4 and Sonnet 4 are
# absent: they reached end-of-life and now return not_found either way.
LEGACY_MODEL_PREFIXES = (
    "claude-3-",
    "claude-haiku-4-5",
    "claude-opus-4-1",
    "claude-opus-4-5",
    "claude-sonnet-4-5",
)

# Server-side refusal fallback is gated far more narrowly than adaptive
# thinking: only models with a published fallback target accept it. Sonnet 5,
# Opus 4.8, and Opus 4.6 all reject it with
# "'<model>' does not support the `fallbacks` parameter", so this one is an
# allow-list -- an unfamiliar model goes without the fallback rather than
# having every request rejected.
FALLBACK_MODEL_PREFIXES = (
    "claude-fable-",
    "claude-mythos-",
    "claude-opus-5",
)

# The `xhigh` effort level arrived with Opus 4.7; Opus 4.6 and Sonnet 4.6
# reject it. Clamp rather than raise -- degrading one level keeps a
# misconfigured graph talking, where an error would drop the whole turn.
NO_XHIGH_MODEL_PREFIXES = ("claude-opus-4-6", "claude-sonnet-4-6")

# Effort levels above `high`, in the order the API ranks them.
ABOVE_HIGH_EFFORTS = ("xhigh", "max")

# Sampling controls the current generation rejects once adaptive thinking is
# on: `temperature` may only be 1, and top_p/top_k are not accepted at all.
# Graphs set them for whichever vendor they were written against --
# main_python sends `temperature: 0.7` on every turn -- so drop them on the
# thinking path rather than fail the request. Older models still accept them.
THINKING_INCOMPATIBLE_PARAMS = ("temperature", "top_p", "top_k")


class AnthropicLLM2Config(BaseModel):
    api_key: str = ""
    base_url: str = ""
    model: str = "claude-opus-5"
    max_tokens: int = 2048
    prompt: str = "You are a helpful assistant."
    proxy_url: str = ""

    # Depth of reasoning and overall token spend. `low` is the default because
    # these graphs are realtime voice assistants where turn latency dominates
    # the experience; raise it for text or agentic graphs.
    effort: Literal["low", "medium", "high", "xhigh", "max"] = "low"

    # Claude surfaces reasoning as `thinking` blocks, which map onto the
    # MESSAGE_REASONING_* events. The API default is "omitted", which streams
    # those blocks with empty text -- so the playground's reasoning pane stays
    # blank unless this is "summarized".
    thinking_display: Literal["summarized", "omitted"] = "summarized"

    # Safety classifiers can decline a request with HTTP 200 and no content.
    # For a voice agent that is silence, so route refusals to a fallback model
    # server-side and, failing that, speak `refusal_message`.
    refusal_fallback: bool = True
    refusal_message: str = "Sorry, I'm not able to help with that request."

    custom_headers: dict[str, Any] = Field(default_factory=dict)
    black_list_params: list[str] = Field(
        default_factory=lambda: [
            "messages",
            "tools",
            "stream",
            "model",
            "system",
            "max_tokens",
        ]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class AnthropicLLM:
    """Adapter between the ten_ai_base LLM contract and the Anthropic Messages
    API.

    Translation runs in both directions: LLMRequest -> Messages request on the
    way in, and Anthropic stream events -> LLMResponse events on the way out.
    Neither side is ours to change, so every quirk is absorbed here.
    """

    def __init__(self, ten_env: AsyncTenEnv, config: AnthropicLLM2Config):
        self.ten_env = ten_env
        self.config = config

        self.http_client = None
        if config.proxy_url:
            ten_env.log_info(f"Setting httpx proxy: {config.proxy_url}")
            self.http_client = httpx.AsyncClient(proxy=config.proxy_url)

        default_headers: dict[str, str] = {}
        for key, value in config.custom_headers.items():
            if isinstance(value, (dict, list)):
                ten_env.log_warn(
                    f"Skipping custom header '{key}':"
                    f" value must be a scalar, got {type(value).__name__}"
                )
                continue
            default_headers[str(key)] = str(value)

        self.client = AsyncAnthropic(
            api_key=config.api_key,
            base_url=config.base_url or DEFAULT_BASE_URL,
            default_headers=default_headers or None,
            http_client=self.http_client,
        )

    async def aclose(self) -> None:
        """Release the connection pools.

        A worker starts and stops graphs repeatedly, so without this each
        cycle leaks a pool -- and with a proxy configured, entries in the
        proxy's connection table too.
        """
        try:
            await self.client.close()
        except Exception as err:
            self.ten_env.log_warn(
                f"Failed to close the Anthropic client: {err}"
            )
        if self.http_client is not None:
            try:
                await self.http_client.aclose()
            except Exception as err:
                self.ten_env.log_warn(f"Failed to close the HTTP client: {err}")

    # ------------------------------------------------------------------
    # Request translation
    # ------------------------------------------------------------------

    def _loads_arguments(self, raw: str) -> dict[str, Any]:
        """ten_ai_base carries tool arguments as a JSON string; Anthropic wants
        a parsed object."""
        if not raw:
            return {}
        try:
            parsed = json.loads(raw)
        except json.JSONDecodeError as err:
            self.ten_env.log_warn(f"Malformed tool arguments {raw!r}: {err}")
            return {}
        return parsed if isinstance(parsed, dict) else {}

    def _image_block(self, url: str) -> dict[str, Any]:
        """ImageURL.url carries both remote URLs and inline data URIs."""
        match = DATA_URI_RE.match(url)
        if match:
            return {
                "type": "image",
                "source": {
                    "type": "base64",
                    "media_type": match.group("media_type"),
                    "data": match.group("data"),
                },
            }
        return {"type": "image", "source": {"type": "url", "url": url}}

    def _content_blocks(self, content: Any) -> Any:
        if isinstance(content, str):
            return content

        blocks: list[dict[str, Any]] = []
        for item in content or []:
            match item:
                case TextContent():
                    blocks.append({"type": "text", "text": item.text})
                case ImageContent():
                    # ImageURL.detail has no Anthropic equivalent.
                    blocks.append(self._image_block(item.image_url.url))
        return blocks

    @staticmethod
    def _is_block_turn(
        message: dict[str, Any], role: str, block_type: str
    ) -> bool:
        """True when `message` is a turn made up only of `block_type` blocks.

        Used to append onto the turn instead of starting a new one, so parallel
        tool use round-trips as a single assistant turn and a single user turn
        rather than one message per call.
        """
        content = message.get("content")
        return (
            message.get("role") == role
            and isinstance(content, list)
            and bool(content)
            and all(block.get("type") == block_type for block in content)
        )

    def _parse_messages(
        self, messages: list[Any], base_prompt: str
    ) -> tuple[str, list[dict[str, Any]]]:
        """Returns (system_prompt, messages).

        Anthropic takes the system prompt as a top-level parameter rather than
        a message, so in-history system turns are folded onto it.
        """
        system_parts = [base_prompt] if base_prompt else []
        parsed: list[dict[str, Any]] = []

        for message in messages:
            match message:
                case LLMMessageContent():
                    if message.role == "system":
                        content = message.content
                        system_parts.append(
                            content
                            if isinstance(content, str)
                            else "\n".join(
                                item.text
                                for item in content or []
                                if isinstance(item, TextContent)
                            )
                        )
                        continue
                    parsed.append(
                        {
                            "role": message.role,
                            "content": self._content_blocks(message.content),
                        }
                    )
                case LLMMessageFunctionCall():
                    block = {
                        "type": "tool_use",
                        "id": message.call_id,
                        "name": message.name,
                        "input": self._loads_arguments(message.arguments),
                    }
                    # Parallel calls arrive as separate messages but came from
                    # one assistant turn, so rebuild that turn.
                    if parsed and self._is_block_turn(
                        parsed[-1], "assistant", "tool_use"
                    ):
                        parsed[-1]["content"].append(block)
                    else:
                        parsed.append({"role": "assistant", "content": [block]})
                case LLMMessageFunctionCallOutput():
                    block = {
                        "type": "tool_result",
                        "tool_use_id": message.call_id,
                        "content": message.output,
                    }
                    # Anthropic wants every result from one assistant turn in a
                    # single user message. Splitting them is accepted but
                    # discourages future parallel tool use, so merge instead.
                    if parsed and self._is_block_turn(
                        parsed[-1], "user", "tool_result"
                    ):
                        parsed[-1]["content"].append(block)
                    else:
                        parsed.append({"role": "user", "content": [block]})

        return "\n\n".join(part for part in system_parts if part), parsed

    def _convert_tool(self, tool: LLMToolMetadata) -> dict[str, Any]:
        properties: dict[str, Any] = {}
        required: list[str] = []

        for param in tool.parameters:
            schema: dict[str, Any] = {
                "type": param.type,
                "description": param.description,
            }
            if param.type == "array" and getattr(param, "items", None):
                schema["items"] = param.items
            properties[param.name] = schema
            if param.required:
                required.append(param.name)

        return {
            "name": tool.name,
            "description": tool.description,
            "input_schema": {
                "type": "object",
                "properties": properties,
                "required": required,
            },
        }

    def _effort_for(self, model: str) -> str:
        """Effort level, narrowed to what `model` accepts."""
        effort = self.config.effort
        # `max` sits above `xhigh`, so a model that predates `xhigh` cannot
        # accept it either. Clamp both to the highest level it does know.
        if effort in ABOVE_HIGH_EFFORTS and model.startswith(
            NO_XHIGH_MODEL_PREFIXES
        ):
            self.ten_env.log_info(
                f"{model} does not support effort '{effort}'; using 'high'"
            )
            return "high"
        return effort

    def _build_request(self, request_input: LLMRequest) -> dict[str, Any]:
        system_prompt, messages = self._parse_messages(
            request_input.messages, request_input.prompt or self.config.prompt
        )

        if not messages:
            raise RuntimeError(
                "Anthropic requires at least one message; the translated "
                "history is empty"
            )
        if messages[0]["role"] != "user":
            raise RuntimeError(
                "Anthropic requires the first message to have role 'user'; "
                f"got '{messages[0]['role']}'"
            )

        # `request_input.model` is deliberately ignored, matching
        # openai_llm2_python. Callers set it for whichever vendor they were
        # written against -- vision_analyze_tool_python hardcodes "gpt-4o" --
        # and honouring it would send that straight to Anthropic as a 404.
        model = self.config.model
        if request_input.model and request_input.model != model:
            self.ten_env.log_debug(
                f"ignoring caller-supplied model '{request_input.model}'; "
                f"using the configured '{model}'"
            )

        req: dict[str, Any] = {
            "model": model,
            "max_tokens": self.config.max_tokens,
            "messages": messages,
        }

        if system_prompt:
            req["system"] = system_prompt

        tools = [
            self._convert_tool(tool) for tool in (request_input.tools or [])
        ]
        if tools:
            req["tools"] = tools

        if model.startswith(LEGACY_MODEL_PREFIXES):
            # Older served models reject these outright, so send a plain
            # request instead. Reasoning events are unavailable there.
            self.ten_env.log_info(
                f"{model} predates adaptive thinking, effort, and refusal "
                "fallback; omitting them from the request"
            )
        else:
            req["thinking"] = {
                "type": "adaptive",
                "display": self.config.thinking_display,
            }
            req["output_config"] = {"effort": self._effort_for(model)}

        if self.config.refusal_fallback:
            if model.startswith(FALLBACK_MODEL_PREFIXES):
                req["betas"] = [FALLBACK_BETA]
                # Passed as raw body rather than a typed kwarg so the
                # extension keeps working on SDK releases that predate the
                # parameter.
                req["extra_body"] = {"fallbacks": "default"}
            else:
                self.ten_env.log_info(
                    f"{model} does not support the fallbacks parameter; "
                    "refusals will surface as refusal_message instead"
                )

        thinking_enabled = "thinking" in req
        for key, value in (request_input.parameters or {}).items():
            if self.config.is_black_list_params(key):
                continue
            if thinking_enabled and key in THINKING_INCOMPATIBLE_PARAMS:
                self.ten_env.log_debug(
                    f"dropping '{key}': adaptive thinking does not accept it"
                )
                continue
            self.ten_env.log_debug(f"set anthropic param: {key} = {value}")
            req[key] = value

        return req

    # ------------------------------------------------------------------
    # Response translation
    # ------------------------------------------------------------------

    def _messages(self, req: dict[str, Any]) -> Any:
        """The messages resource this request needs.

        Only refusal fallback requires the beta surface, so a graph that
        turns it off stays on /v1/messages -- which is what an operator
        disabling it for a gateway or compliance reason is asking for.
        """
        if "betas" in req:
            return self.client.beta.messages
        return self.client.messages

    def _log_refusal(self, message: Any) -> None:
        details = getattr(message, "stop_details", None)
        category = getattr(details, "category", None) if details else None
        self.ten_env.log_info(
            f"Request refused by safety classifiers, category={category}"
        )

    async def get_chat_completions(
        self, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        req = self._build_request(request_input)

        self.ten_env.log_info(
            f"get_chat_completions: {len(req['messages'])} messages, "
            f"streaming: {request_input.streaming}, model: {req['model']}"
        )

        if request_input.streaming:
            async for response in self._stream(req):
                yield response
        else:
            async for response in self._complete(req):
                yield response

    async def _stream(
        self, req: dict[str, Any]
    ) -> AsyncGenerator[LLMResponse, None]:
        created = int(time.time())
        response_id = ""
        text_content = ""
        reasoning_content = ""
        block_types: dict[int, str] = {}
        tool_blocks: dict[int, dict[str, str]] = {}

        try:
            async with self._messages(req).stream(**req) as stream:
                async for event in stream:
                    match event.type:
                        case "message_start":
                            response_id = event.message.id

                        case "content_block_start":
                            block = event.content_block
                            block_types[event.index] = block.type
                            if block.type == "tool_use":
                                tool_blocks[event.index] = {
                                    "id": block.id,
                                    "name": block.name,
                                    "json": "",
                                }

                        case "content_block_delta":
                            delta = event.delta
                            match delta.type:
                                case "text_delta":
                                    text_content += delta.text
                                    yield LLMResponseMessageDelta(
                                        response_id=response_id,
                                        role="assistant",
                                        content=text_content,
                                        delta=delta.text,
                                        created=created,
                                    )
                                case "thinking_delta":
                                    reasoning_content += delta.thinking
                                    yield LLMResponseReasoningDelta(
                                        response_id=response_id,
                                        role="assistant",
                                        content=reasoning_content,
                                        delta=delta.thinking,
                                        created=created,
                                    )
                                case "input_json_delta":
                                    buffered = tool_blocks.get(event.index)
                                    if buffered is not None:
                                        buffered["json"] += delta.partial_json

                        case "content_block_stop":
                            block_type = block_types.pop(event.index, None)
                            if block_type == "tool_use":
                                buffered = tool_blocks.pop(event.index, None)
                                if buffered is not None:
                                    yield LLMResponseToolCall(
                                        response_id=response_id,
                                        id=response_id,
                                        tool_call_id=buffered["id"],
                                        name=buffered["name"],
                                        arguments=self._loads_arguments(
                                            buffered["json"]
                                        ),
                                        created=created,
                                    )
                            elif block_type == "thinking" and reasoning_content:
                                yield LLMResponseReasoningDone(
                                    response_id=response_id,
                                    role="assistant",
                                    content=reasoning_content,
                                    created=created,
                                )
                                reasoning_content = ""

                final_message = await stream.get_final_message()
        except Exception as err:
            raise RuntimeError(f"CreateMessage failed, err: {err}") from err

        # Check stop_reason before trusting content: a pre-output refusal
        # returns an empty content list.
        if final_message.stop_reason == "refusal":
            self._log_refusal(final_message)
            yield LLMResponseMessageDone(
                response_id=response_id or final_message.id,
                role="assistant",
                content=self.config.refusal_message,
                created=created,
            )
            return

        yield LLMResponseMessageDone(
            response_id=response_id or final_message.id,
            role="assistant",
            content=text_content,
            created=created,
        )

    async def _complete(
        self, req: dict[str, Any]
    ) -> AsyncGenerator[LLMResponse, None]:
        created = int(time.time())

        try:
            message = await self._messages(req).create(**req)
        except Exception as err:
            raise RuntimeError(f"CreateMessage failed, err: {err}") from err

        if message.stop_reason == "refusal":
            self._log_refusal(message)
            yield LLMResponseMessageDone(
                response_id=message.id,
                role="assistant",
                content=self.config.refusal_message,
                created=created,
            )
            return

        reasoning_content = "".join(
            getattr(block, "thinking", "") or ""
            for block in message.content
            if block.type == "thinking"
        )
        if reasoning_content:
            yield LLMResponseReasoningDelta(
                response_id=message.id,
                role="assistant",
                content=reasoning_content,
                delta=reasoning_content,
                created=created,
            )
            yield LLMResponseReasoningDone(
                response_id=message.id,
                role="assistant",
                content=reasoning_content,
                created=created,
            )

        text_content = "".join(
            block.text for block in message.content if block.type == "text"
        )
        if text_content:
            yield LLMResponseMessageDelta(
                response_id=message.id,
                role="assistant",
                content=text_content,
                delta=text_content,
                created=created,
            )

        for block in message.content:
            if block.type == "tool_use":
                yield LLMResponseToolCall(
                    response_id=message.id,
                    id=message.id,
                    tool_call_id=block.id,
                    name=block.name,
                    arguments=(
                        block.input if isinstance(block.input, dict) else {}
                    ),
                    created=created,
                )

        yield LLMResponseMessageDone(
            response_id=message.id,
            role="assistant",
            content=text_content,
            created=created,
        )
