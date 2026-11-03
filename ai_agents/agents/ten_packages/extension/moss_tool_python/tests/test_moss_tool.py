"""Unit tests for moss_tool_python, with the TEN runtime and Moss stubbed."""

import asyncio
import json
import sys
import types

import pytest


class Stub:
    """Keeps its keyword arguments as attributes."""

    def __init__(self, **fields):
        self.__dict__.update(fields)


class FakeToolBase:
    """Records the tools that on_start registers with main_control."""

    def __init__(self, _name):
        self.registered = []

    def get_tool_metadata(self, _ten_env):
        return []

    async def on_start(self, ten_env):
        self.registered = [t.name for t in self.get_tool_metadata(ten_env)]

    async def on_stop(self, _ten_env):
        pass

    async def call_tool(self, ten_env, name, args):
        """Runs a tool call as the real on_cmd does, catching only Exception."""
        try:
            return await asyncio.create_task(self.run_tool(ten_env, name, args))
        except Exception:
            return "error"


class FakeClient:
    """Plays back an index load and a search, and records each search."""

    gate = None
    load_error = None
    query_error = None

    def __init__(self, _project_id, _project_key):
        self.queries = []
        self.closed = False
        self.auto_refresh = False

    async def load_index(self, _name, auto_refresh=False):
        self.auto_refresh = auto_refresh
        if FakeClient.gate:
            await FakeClient.gate.wait()
        if FakeClient.load_error:
            raise FakeClient.load_error

    async def query(self, name, query, options):
        self.queries.append((name, query, options.top_k))
        if FakeClient.query_error:
            raise FakeClient.query_error
        docs = [Stub(text="Parking is on P3."), Stub(text="Show the token.")]
        return Stub(docs=docs, time_taken_ms=1)

    async def close(self):
        self.closed = True


class FakeEnv:
    def __init__(self, properties):
        self.properties = properties
        self.errors = []

    async def get_property_to_json(self, _path):
        return json.dumps(self.properties), None

    def log_info(self, _message):
        pass

    def log_error(self, message):
        self.errors.append(message)


runtime = types.ModuleType("ten_runtime")
runtime.Addon = object
runtime.AsyncTenEnv = object
runtime.TenEnv = object
runtime.register_addon_as_extension = lambda _name: lambda addon: addon
llm_tool = types.ModuleType("ten_ai_base.llm_tool")
llm_tool.AsyncLLMToolBaseExtension = FakeToolBase
ai_types = types.ModuleType("ten_ai_base.types")
ai_types.LLMToolMetadata = Stub
ai_types.LLMToolMetadataParameter = Stub
ai_types.LLMToolResult = dict
moss = types.ModuleType("moss")
moss.MossClient = FakeClient
moss.QueryOptions = Stub
sys.modules.update(
    {
        "ten_runtime": runtime,
        "ten_ai_base": types.ModuleType("ten_ai_base"),
        "ten_ai_base.llm_tool": llm_tool,
        "ten_ai_base.types": ai_types,
        "moss": moss,
    }
)

from ten_packages.extension.moss_tool_python.extension import (  # noqa: E402
    TOOL_NAME,
    MossToolExtension,
)

PROPERTIES = {"project_id": "p", "project_key": "secret", "index_name": "faq"}
NO_PASSAGES = {"type": "llmresult", "content": ""}


@pytest.fixture(autouse=True)
def reset_client():
    FakeClient.gate = None
    FakeClient.load_error = None
    FakeClient.query_error = None


async def start_tool(**properties):
    tool = MossToolExtension("moss")
    env = FakeEnv({**PROPERTIES, **properties})
    await tool.on_start(env)
    return tool, env


@pytest.mark.asyncio
async def test_search_waits_for_the_index_load():
    FakeClient.gate = asyncio.Event()
    tool, env = await start_tool(top_k=5)
    assert tool.registered == [TOOL_NAME]

    search = asyncio.create_task(
        tool.run_tool(env, TOOL_NAME, {"query": "where do I park"})
    )
    await asyncio.sleep(0)
    assert not search.done()

    FakeClient.gate.set()
    assert await search == {
        "type": "llmresult",
        "content": "Parking is on P3.\n\nShow the token.",
    }
    assert tool.client.queries == [("faq", "where do I park", 5)]
    assert tool.client.auto_refresh


@pytest.mark.asyncio
async def test_failed_load_returns_no_passages():
    FakeClient.load_error = RuntimeError("credentials rejected")
    tool, env = await start_tool()
    assert tool.registered == [TOOL_NAME]

    assert await tool.run_tool(env, TOOL_NAME, {"query": "hours"}) == (
        NO_PASSAGES
    )
    assert tool.client.queries == []
    assert len(env.errors) == 1 and "secret" not in env.errors[0]


@pytest.mark.asyncio
async def test_other_tools_and_empty_queries_skip_the_search():
    tool, env = await start_tool()
    await tool.loading

    assert await tool.run_tool(env, "other", {"query": "hours"}) == (
        NO_PASSAGES
    )
    assert await tool.run_tool(env, TOOL_NAME, {"query": ""}) == NO_PASSAGES
    assert await tool.run_tool(env, TOOL_NAME, {}) == NO_PASSAGES
    assert tool.client.queries == []


@pytest.mark.asyncio
async def test_failed_search_returns_no_passages():
    FakeClient.query_error = RuntimeError("index closed")
    tool, env = await start_tool()

    assert await tool.run_tool(env, TOOL_NAME, {"query": "hours"}) == (
        NO_PASSAGES
    )
    assert env.errors == ["Moss search failed: index closed"]


@pytest.mark.asyncio
async def test_stop_during_load_answers_waiting_searches():
    FakeClient.gate = asyncio.Event()
    tool, env = await start_tool()
    search = asyncio.create_task(
        tool.call_tool(env, TOOL_NAME, {"query": "hours"})
    )
    for _ in range(3):
        await asyncio.sleep(0)
    assert not search.done()

    await tool.on_stop(env)
    assert await search == NO_PASSAGES
    assert tool.client.closed


@pytest.mark.asyncio
async def test_stop_closes_the_client():
    tool, env = await start_tool()
    await tool.loading

    await tool.on_stop(env)
    assert tool.client.closed
