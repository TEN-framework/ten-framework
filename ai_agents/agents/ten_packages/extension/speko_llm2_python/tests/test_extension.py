import inspect
import json
from unittest.mock import AsyncMock, MagicMock, patch

import httpx
import pytest
from ten_ai_base.message import ModuleErrorCode
from ten_ai_base.struct import LLMRequest
from ten_ai_base.types import LLMToolMetadata, LLMToolMetadataParameter

from speko_llm2_python.client import SpekoLLMClient, SSEEvent
from speko_llm2_python.config import SpekoLLM2Config
from speko_llm2_python.extension import SpekoLLM2Extension


@pytest.mark.asyncio
@pytest.mark.parametrize("streaming", [False, True])
async def test_completion_interface_and_explicit_route(streaming):
    extension = SpekoLLM2Extension("speko")
    extension.config = SpekoLLM2Config(
        api_key="key",
        routing={
            "mode": "explicit",
            "objective": "balanced",
            "provider": "openai",
            "model": "test-model",
        },
    )
    extension.config.validate_required()
    extension.ten_env = MagicMock(send_data=AsyncMock())
    captured = []

    async def stream(payload, **_kwargs):
        captured.append(payload)
        yield SSEEvent("response.created", {"response_id": "response"})
        yield SSEEvent("response.text.delta", {"delta": "hello"})
        yield SSEEvent("response.completed", {"usage": {"output_tokens": 1}})

    async def complete(payload, **_kwargs):
        captured.append(payload)
        return {
            "id": "response",
            "output": [
                {
                    "type": "message",
                    "role": "assistant",
                    "content": [{"type": "text", "text": "hello"}],
                }
            ],
            "usage": {"output_tokens": 1},
        }

    extension.client = MagicMock(
        stream=stream, complete=complete, route={"provider": "openai"}
    )
    request = LLMRequest(request_id="request", messages=[], streaming=streaming)
    iterator = extension.on_call_chat_completion(extension.ten_env, request)
    assert inspect.isasyncgen(iterator)
    responses = [response async for response in iterator]
    assert responses[-1].content == "hello"
    assert responses[-1].response_id == "response"
    assert captured[0]["routing"] == {
        "mode": "explicit",
        "provider": "openai",
        "model": "test-model",
    }
    raw, _ = extension.ten_env.send_data.await_args.args[
        0
    ].get_property_to_json("")
    assert json.loads(raw)["metadata"]["request_id"] == "request"


@pytest.mark.asyncio
@pytest.mark.parametrize("close_fails", [False, True])
async def test_stop_cancels_requests_and_closes_client_before_base(
    close_fails,
):
    extension = SpekoLLM2Extension("speko")
    calls = []

    async def cancel_all():
        calls.append("cancel")

    async def close():
        calls.append("close")
        if close_fails:
            raise RuntimeError("close failure")

    async def stop(_ten_env):
        calls.append("base")

    extension._cancel_all = cancel_all
    extension.client = MagicMock(close=AsyncMock(side_effect=close))
    ten_env = MagicMock()
    with patch(
        "speko_llm2_python.extension.AsyncLLM2BaseExtension.on_stop",
        side_effect=stop,
    ):
        # An exception escaping on_stop would abort the TEN process.
        await extension.on_stop(ten_env)
    assert calls == ["cancel", "close", "base"]
    assert extension.client is None
    assert ten_env.log_warn.called is close_fails


def _configured_extension(handler):
    extension = SpekoLLM2Extension("speko")
    extension.config = SpekoLLM2Config(api_key="key")
    extension.config.validate_required()
    extension.ten_env = MagicMock(send_data=AsyncMock())
    extension.client = SpekoLLMClient(
        api_key="key",
        base_url="https://router.speko.dev",
        timeout_sec=1,
        http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)),
    )
    return extension


def _sse(*events):
    return "".join(
        f"event: {name}\ndata: {json.dumps(data)}\n\n" for name, data in events
    )


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "streaming,response,router_code",
    [
        # Malformed 2xx JSON.
        (False, httpx.Response(200, text="not json"), "relay_error"),
        # {"error": "..."} envelopes over HTTP and SSE.
        (False, httpx.Response(503, json={"error": "busy"}), "relay_error"),
        (
            True,
            httpx.Response(200, text=_sse(("error", {"error": "busy"}))),
            "relay_error",
        ),
        # A payload the client accepts but the extension cannot use.
        (
            False,
            httpx.Response(
                200,
                json={
                    "id": "response",
                    "output": [
                        {
                            "type": "function_call",
                            "call_id": "call",
                            "name": "lookup",
                            "arguments": "{not json",
                        }
                    ],
                },
            ),
            "relay_error",
        ),
        # An invalid TEN request is reported, not only logged.
        (False, None, "invalid_request"),
    ],
)
async def test_failures_emit_ten_error_with_speko_vendor_info(
    streaming, response, router_code
):
    extension = _configured_extension(lambda request: response)
    request = LLMRequest(
        request_id="request",
        messages=[],
        streaming=streaming,
        model="bare-model" if response is None else None,
    )
    with pytest.raises(RuntimeError):
        _ = [
            item
            async for item in extension.on_call_chat_completion(
                extension.ten_env, request
            )
        ]
    sent = [
        call.args[0] for call in extension.ten_env.send_data.await_args_list
    ]
    errors = [data for data in sent if data.get_name() == "error"]
    assert len(errors) == 1
    raw, _ = errors[0].get_property_to_json("")
    payload = json.loads(raw)
    assert payload["id"] == "request"
    assert payload["code"] == int(ModuleErrorCode.NON_FATAL_ERROR.value)
    assert payload["vendor_info"]["vendor"] == "speko"
    assert payload["vendor_info"]["code"] == router_code
    await extension.client._client.aclose()


class MCPToolParameter(LLMToolMetadataParameter):
    """A tool parameter as MCP supplies it, with its array element schema."""

    items: dict | None = None


@pytest.mark.asyncio
async def test_array_tool_parameter_keeps_its_item_schema_in_the_request():
    sent = []

    def handler(request):
        sent.append(json.loads(request.content))
        return httpx.Response(
            200,
            json={
                "id": "response",
                "output": [
                    {
                        "type": "message",
                        "role": "assistant",
                        "content": [{"type": "text", "text": "ok"}],
                    }
                ],
            },
        )

    extension = _configured_extension(handler)
    items = {
        "type": "object",
        "properties": {"tag": {"type": "string"}},
        "required": ["tag"],
    }
    tool = LLMToolMetadata(
        name="search",
        description="Search notes",
        parameters=[
            MCPToolParameter(
                name="filters",
                type="array",
                description="Tag filters",
                required=True,
                items=items,
            ),
            LLMToolMetadataParameter(
                name="query", type="string", description="Text", required=True
            ),
        ],
    )
    request = LLMRequest(
        request_id="request", messages=[], streaming=False, tools=[tool]
    )
    _ = [
        item
        async for item in extension.on_call_chat_completion(
            extension.ten_env, request
        )
    ]
    properties = sent[0]["tools"][0]["parameters"]["properties"]
    assert properties["filters"] == {
        "type": "array",
        "description": "Tag filters",
        "items": items,
    }
    assert "items" not in properties["query"]
    await extension.client._client.aclose()


def test_config_log_redacts_url():
    config = SpekoLLM2Config(
        api_key="test-secret-key-long",
        base_url="https://user:password@example.com?signature=private",
    )
    config.validate_required()
    for secret in ("test-secret-key-long", "password", "private"):
        assert secret not in config.to_str()
