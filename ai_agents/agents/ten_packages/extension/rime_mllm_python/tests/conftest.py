"""Local WebSocket peers and an isolated TEN runtime double."""

import asyncio
import importlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import types
from enum import IntEnum

from aiohttp import web
import pytest
import pytest_asyncio

sys.path.insert(0, str(Path(__file__).resolve().parents[4]))
# Transport tests load the package without importing TEN's native addon entry.
protocol_package = types.ModuleType("prism_test_protocol")
protocol_package.__path__ = [str(Path(__file__).resolve().parents[1])]
protocol_package.__file__ = str(Path(protocol_package.__path__[0]) / "__init__.py")
sys.modules[protocol_package.__name__] = protocol_package
# Pytest imports the enclosing package during setup, before runtime fixtures.
sys.modules["rime_mllm_python"] = protocol_package


class PrismPeer:
    """A local server whose acknowledgments are controlled by the test."""

    def __init__(self):
        self.requests = asyncio.Queue()
        self.connected = asyncio.Future()
        self.authorizations = []
        self.auto_initialize = True
        self.refuse_status = None
        self.url = None

    async def handle(self, request):
        self.authorizations.append(request.headers.get("Authorization"))
        if self.refuse_status:
            return web.Response(status=self.refuse_status)
        socket = web.WebSocketResponse()
        await socket.prepare(request)
        self.connected.set_result(socket)
        await socket.send_json(
            {
                "type": "session.created",
                "session": {"id": "session", "model": "prism"},
            }
        )
        async for message in socket:
            event = json.loads(message.data)
            await self.requests.put(event)
            if event["type"] == "session.update" and self.auto_initialize:
                await self.acknowledge(event, "session.updated")
        return socket

    async def send(self, event):
        socket = await self.connected
        await socket.send_json(event)

    async def acknowledge(self, request, kind):
        await self.send({"type": kind, "prism_request_event_id": request["event_id"]})

    async def request(self, kind):
        while True:
            request = await asyncio.wait_for(self.requests.get(), 2)
            if request["type"] == kind:
                return request


@pytest_asyncio.fixture
async def peer():
    server = PrismPeer()
    app = web.Application()
    app.router.add_get("/v1/realtime", server.handle)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    server.url = f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}/v1/realtime".replace(
        "http:", "ws:"
    )
    yield server
    if server.connected.done():
        await server.connected.result().close()
    await runner.cleanup()


@pytest_asyncio.fixture
async def connection(peer):
    from prism_test_protocol.config import (
        RimeRealtimeConfig,
    )
    from prism_test_protocol.connection import (
        PrismConnection,
    )

    events = asyncio.Queue()
    client = PrismConnection(
        RimeRealtimeConfig(endpoint=peer.url, api_key="test-only", startup_retries=0),
        events.put,
    )
    yield client, events
    await client.close()


class FakeMessage:
    """Properties and PCM storage matching the TEN methods used by the adapter."""

    def __init__(self, name="", payload=None):
        self.name = name
        self.payload = payload or {}
        self.buffer = bytearray()
        self.sample_rate = 16000
        self.channels = 1
        self.width = 2

    @classmethod
    def create(cls, name, *_args):
        return cls(name)

    def get_name(self):
        return self.name

    def set_property_from_json(self, path, value):
        if path in {None, ""}:
            self.payload = json.loads(value)
        else:
            self.payload[path] = json.loads(value)

    def get_property_to_json(self, path):
        value = self.payload if path in {None, ""} else self.payload.get(path)
        return (json.dumps(value) if value is not None else ""), None

    def set_dests(self, _destinations):
        pass

    def get_sample_rate(self):
        return self.sample_rate

    def set_sample_rate(self, value):
        self.sample_rate = value

    def get_number_of_channels(self):
        return self.channels

    def set_number_of_channels(self, value):
        self.channels = value

    def get_bytes_per_sample(self):
        return self.width

    def set_bytes_per_sample(self, value):
        self.width = value

    def set_data_fmt(self, _value):
        pass

    def set_samples_per_channel(self, _value):
        pass

    def alloc_buf(self, size):
        self.buffer = bytearray(size)

    def get_buf(self):
        return self.buffer

    def lock_buf(self):
        return self.buffer

    def unlock_buf(self, _buffer):
        pass


class StatusCode(IntEnum):
    OK = 0
    ERROR = 1


class FakeResult(FakeMessage):
    def get_status_code(self):
        return self.name


class FakeEnv:
    def __init__(self, properties):
        self.properties = properties
        self.data = asyncio.Queue()
        self.audio = asyncio.Queue()
        self.commands = asyncio.Queue()
        self.logs = []
        self.tool_gate = asyncio.Event()
        self.created = None

    def on_create_instance_done(self, instance, context):
        self.created = instance, context

    async def get_property_to_json(self, _path):
        return json.dumps(self.properties), None

    async def send_data(self, data):
        await self.data.put(data)

    async def send_audio_frame(self, frame):
        await self.audio.put(frame)

    async def send_cmd(self, command):
        await self.commands.put(command)
        if command.name != "flush":
            await self.tool_gate.wait()
        return FakeResult(StatusCode.OK), None

    def log_info(self, text):
        self.logs.append(text)

    log_debug = log_info
    log_warn = log_info
    log_error = log_info


@pytest.fixture
def ten_modules():
    """Use real TEN base models/helpers with a double only for native runtime."""
    base_path = os.environ.get("TEN_AI_BASE_INTERFACE_PATH")
    if base_path is None:
        specification = importlib.util.find_spec("ten_ai_base")
        if specification and specification.submodule_search_locations:
            base_path = str(
                Path(next(iter(specification.submodule_search_locations))).parent
            )
    if base_path is None:
        pytest.skip("Install TEN dependencies or set TEN_AI_BASE_INTERFACE_PATH")
    prefixes = (
        "ten_runtime",
        "ten_ai_base",
        "ten_packages.extension.rime_mllm_python",
    )
    previous = {
        name: module
        for name, module in sys.modules.items()
        if name.startswith(prefixes)
    }
    for name in previous:
        del sys.modules[name]
    runtime = types.ModuleType("ten_runtime")
    registered = {}

    def register(name):
        def decorate(addon):
            registered[name] = addon
            return addon

        return decorate

    runtime.Addon = type("Addon", (), {})
    runtime.register_addon_as_extension = register
    runtime.AsyncExtension = type(
        "AsyncExtension", (), {"__init__": lambda self, name: None}
    )
    runtime.AsyncTenEnv = FakeEnv
    runtime.TenEnv = FakeEnv
    runtime.AudioFrame = FakeMessage
    runtime.Cmd = FakeMessage
    runtime.Data = FakeMessage
    runtime.CmdResult = FakeResult
    runtime.StatusCode = StatusCode
    runtime.AudioFrameDataFmt = types.SimpleNamespace(INTERLEAVE=0)
    runtime.Loc = lambda *args: args
    runtime.TenError = RuntimeError
    sys.modules["ten_runtime"] = runtime
    async_environment = types.ModuleType("ten_runtime.async_ten_env")
    async_environment.AsyncTenEnv = FakeEnv
    sys.modules[async_environment.__name__] = async_environment
    package = types.ModuleType("ten_ai_base")
    package.__path__ = [str(Path(base_path) / "ten_ai_base")]
    sys.modules["ten_ai_base"] = package
    try:
        yield types.SimpleNamespace(
            extension=importlib.import_module(
                "ten_packages.extension.rime_mllm_python.extension"
            ),
            environment=FakeEnv,
            message=FakeMessage,
            registered=registered,
        )
    finally:
        for name in list(sys.modules):
            if name.startswith(prefixes):
                del sys.modules[name]
        sys.modules.update(previous)
