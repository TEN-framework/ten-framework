#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""End-to-end tests that drive the extension through the TEN runtime.

A direct unit test of `AnthropicLLM` can only prove the translation is
right. These load the addon the way a graph does -- registration through
`addon.py`, the `llm-interface.json` import in `manifest.json`, property
injection, and the `chat_completion` cmd dispatched by
`AsyncLLM2BaseExtension` -- and assert on the streamed `CmdResult`s exactly
as `main_cascade_python` consumes them.

Only `test_extension_loads_and_dispatches` runs without credentials. The
rest talk to the real Anthropic API and are skipped unless a key is set:

    export ANTHROPIC_API_KEY=sk-ant-...
    task test-extension \
      EXTENSION=agents/ten_packages/extension/anthropic_llm2_python
"""

import asyncio
import json
import os

import pytest

from anthropic_llm2_python.anthropic_llm import LEGACY_MODEL_PREFIXES
from ten_ai_base.struct import (
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
    parse_llm_response,
)
from ten_ai_base.types import LLMToolMetadata, LLMToolMetadataParameter
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    StatusCode,
)

ADDON_NAME = "anthropic_llm2_python"
API_KEY = os.environ.get("ANTHROPIC_API_KEY", "")
MODEL = os.environ.get("ANTHROPIC_MODEL") or "claude-opus-5"
LEGACY_MODEL = "claude-haiku-4-5"

requires_api_key = pytest.mark.skipif(
    not API_KEY, reason="ANTHROPIC_API_KEY is not set"
)

# Adaptive thinking only engages when the task needs it, so a question the
# model can answer from memory yields no reasoning events at any effort.
THINKING_PROMPT = (
    "Compute 17 * 23 * 31 step by step, then tell me the sum of the digits "
    "of the result."
)

WEATHER_TOOL = LLMToolMetadata(
    name="get_weather",
    description="Get the current temperature for a city.",
    parameters=[
        LLMToolMetadataParameter(
            name="city",
            type="string",
            description="City name",
            required=True,
        )
    ],
)


def user(text: str) -> LLMMessageContent:
    return LLMMessageContent(role="user", content=text)


def properties(**overrides) -> str:
    """The same shape `property.json` resolves to, minus the env lookups."""
    props = {
        "api_key": API_KEY,
        "model": MODEL,
        "max_tokens": 2048,
        "prompt": "",
        "effort": "low",
        "thinking_display": "summarized",
        "refusal_fallback": True,
    }
    props.update(overrides)
    return json.dumps(props)


class ChatCompletionTester(AsyncExtensionTester):
    """Sends one `chat_completion` and collects the streamed responses.

    Mirrors `main_cascade_python.agent.llm_exec`: results that are not yet
    completed carry an `LLMResponse` payload, and the completed one carries
    only the status.
    """

    def __init__(self, request: LLMRequest, timeout: float = 90.0) -> None:
        super().__init__()
        self.request = request
        self.timeout = timeout
        self.responses: list[LLMResponse] = []
        self.final_status: StatusCode | None = None
        self.error: str | None = None
        self._watchdog: asyncio.Task | None = None

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        # Held in an attribute: the loop keeps only a weak reference to
        # pending tasks, so a bare create_task can be collected mid-sleep and
        # a hung request would block instead of failing.
        self._watchdog = asyncio.create_task(self._abort_on_timeout(ten_env))

        cmd = Cmd.create("chat_completion")
        cmd.set_property_from_json(None, self.request.model_dump_json())

        async for result, err in ten_env.send_cmd_ex(cmd):
            if err is not None:
                self.error = f"send_cmd_ex failed: {err.error_message()}"
                break
            if result is None:
                continue
            if result.is_completed():
                self.final_status = result.get_status_code()
                continue

            payload, payload_err = result.get_property_to_json(None)
            if payload_err is not None:
                self.error = f"unreadable result payload: {payload_err}"
                break
            self.responses.append(parse_llm_response(payload))

        if self._watchdog is not None:
            self._watchdog.cancel()
        ten_env.stop_test()

    async def _abort_on_timeout(self, ten_env: AsyncTenEnvTester) -> None:
        await asyncio.sleep(self.timeout)
        self.error = f"timed out after {self.timeout}s"
        ten_env.stop_test()

    # -- accessors -----------------------------------------------------

    def deltas(self) -> list[LLMResponseMessageDelta]:
        return [
            r for r in self.responses if isinstance(r, LLMResponseMessageDelta)
        ]

    def done(self) -> LLMResponseMessageDone | None:
        for response in self.responses:
            if isinstance(response, LLMResponseMessageDone):
                return response
        return None

    def reasoning(self) -> list[LLMResponse]:
        return [
            r
            for r in self.responses
            if isinstance(
                r, (LLMResponseReasoningDelta, LLMResponseReasoningDone)
            )
        ]

    def tool_calls(self) -> list[LLMResponseToolCall]:
        return [r for r in self.responses if isinstance(r, LLMResponseToolCall)]


def run_tester(tester: ChatCompletionTester, **overrides) -> None:
    tester.set_test_mode_single(ADDON_NAME, properties(**overrides))
    err = tester.run()
    assert err is None, err.error_message()
    assert tester.error is None, tester.error


def test_extension_loads_and_dispatches():
    """Runs without credentials.

    Covers what a unit test of the adapter cannot reach: the addon
    registers under its manifest name, the `llm-interface.json` import
    resolves, the single-extension graph builds, properties are injected,
    and `chat_completion` reaches `on_call_chat_completion`. The key is
    deliberately invalid, so the only way to get a completed ERROR result
    is for all of that to have worked and the API call to have failed.
    """
    tester = ChatCompletionTester(
        LLMRequest(request_id="load-check", messages=[user("hi")]),
        timeout=45.0,
    )
    run_tester(tester, api_key="sk-ant-invalid-key-used-by-the-load-check")

    assert tester.final_status == StatusCode.ERROR, (
        "expected the invalid key to be rejected inside the extension; got "
        f"status {tester.final_status} after {len(tester.responses)} responses"
    )


@requires_api_key
def test_streaming_text():
    tester = ChatCompletionTester(
        LLMRequest(
            request_id="stream-text",
            messages=[user("Name one planet, in one short sentence.")],
        )
    )
    run_tester(tester)

    assert tester.final_status == StatusCode.OK
    deltas = tester.deltas()
    assert len(deltas) > 1, f"expected a stream, got {len(deltas)} delta(s)"

    done = tester.done()
    assert done is not None, "no message_content_done event"
    assert done.content and done.content.strip()
    # The adapter accumulates, so every delta carries the text so far.
    assert deltas[-1].content == done.content


@requires_api_key
def test_graph_supplied_sampling_parameters():
    """`main_python` sends `parameters={"temperature": 0.7}` on every turn.

    Forwarding it verbatim is a 400 -- adaptive thinking allows only
    `temperature: 1` -- which would fail every turn of a real voice graph.
    The adapter drops the conflicting keys on the thinking path.
    """
    tester = ChatCompletionTester(
        LLMRequest(
            request_id="graph-parameters",
            messages=[user("Name one planet, in one short sentence.")],
            parameters={"temperature": 0.7, "top_p": 0.9},
        )
    )
    run_tester(tester)

    assert tester.final_status == StatusCode.OK, (
        "sampling parameters set by the graph were forwarded to a model that "
        "rejects them"
    )
    done = tester.done()
    assert done is not None and done.content


@requires_api_key
def test_legacy_model_keeps_sampling_parameters():
    """The same keys are valid without thinking, so they must survive."""
    tester = ChatCompletionTester(
        LLMRequest(
            request_id="legacy-parameters",
            messages=[user("Name one planet, in one short sentence.")],
            parameters={"temperature": 0.2},
        )
    )
    run_tester(tester, model=LEGACY_MODEL)

    assert tester.final_status == StatusCode.OK
    assert tester.done() is not None


@requires_api_key
@pytest.mark.skipif(
    MODEL.startswith(LEGACY_MODEL_PREFIXES),
    reason=f"{MODEL} does not support adaptive thinking",
)
def test_reasoning_reaches_the_graph():
    tester = ChatCompletionTester(
        LLMRequest(request_id="reasoning", messages=[user(THINKING_PROMPT)]),
        timeout=120.0,
    )
    run_tester(tester, max_tokens=3000)

    assert tester.final_status == StatusCode.OK
    assert (
        tester.reasoning()
    ), "no reasoning events -- thinking blocks are not reaching the graph"
    assert tester.done() is not None


@requires_api_key
@pytest.mark.skipif(
    MODEL.startswith(LEGACY_MODEL_PREFIXES),
    reason=f"{MODEL} does not support adaptive thinking",
)
def test_thinking_display_omitted_suppresses_reasoning():
    tester = ChatCompletionTester(
        LLMRequest(
            request_id="reasoning-off", messages=[user(THINKING_PROMPT)]
        ),
        timeout=120.0,
    )
    run_tester(tester, max_tokens=3000, thinking_display="omitted")

    assert tester.final_status == StatusCode.OK
    assert not tester.reasoning()
    done = tester.done()
    assert done is not None and done.content


@requires_api_key
def test_tool_call_round_trip():
    """Two turns through the graph.

    The second one is the highest-risk translation in the adapter: the two
    tool calls and their two results each collapse into a single Anthropic
    turn, and only the real API can confirm the merged shape is accepted.
    """
    question = "What is the weather in Taipei and in Tokyo?"
    first = ChatCompletionTester(
        LLMRequest(
            request_id="tool-call",
            messages=[user(question)],
            tools=[WEATHER_TOOL],
        )
    )
    run_tester(
        first, prompt="Use get_weather for every city you are asked about."
    )

    calls = first.tool_calls()
    assert (
        calls
    ), f"no tool call in {[type(r).__name__ for r in first.responses]}"
    assert all(isinstance(c.arguments, dict) and c.arguments for c in calls)

    history: list = [user(question)]
    for call in calls:
        history.append(
            LLMMessageFunctionCall(
                type="function_call",
                id=call.tool_call_id,
                call_id=call.tool_call_id,
                name=call.name,
                arguments=json.dumps(call.arguments),
            )
        )
    for call in calls:
        city = str(call.arguments.get("city", ""))
        history.append(
            LLMMessageFunctionCallOutput(
                type="function_call_output",
                call_id=call.tool_call_id,
                output="28C" if "Taipei" in city else "19C",
            )
        )

    second = ChatCompletionTester(
        LLMRequest(
            request_id="tool-result",
            messages=history,
            tools=[WEATHER_TOOL],
        )
    )
    run_tester(
        second, prompt="Use get_weather for every city you are asked about."
    )

    assert second.final_status == StatusCode.OK
    done = second.done()
    assert done is not None and done.content and done.content.strip()


@requires_api_key
def test_legacy_model_path():
    """A currently-served older model must take the degraded path.

    No thinking, no effort, no fallback -- but text and tools still work.
    Sending the modern parameters to it is a 400, so this is the assertion
    that the capability gating actually holds inside the runtime.
    """
    tester = ChatCompletionTester(
        LLMRequest(
            request_id="legacy",
            messages=[user("What is the weather in Taipei?")],
            tools=[WEATHER_TOOL],
        )
    )
    run_tester(tester, model=LEGACY_MODEL, prompt="Be brief.")

    assert (
        tester.final_status == StatusCode.OK
    ), "the legacy model rejected the request -- capability gating failed"
    assert tester.tool_calls() or tester.done() is not None
    assert (
        not tester.reasoning()
    ), "legacy models cannot think; reasoning events must not appear"
