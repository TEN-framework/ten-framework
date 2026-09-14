import asyncio
import time
from unittest.mock import AsyncMock

import pytest
from aiohttp import web
from ten_packages.extension.rtzr_asr_python.client import RTZRClient

from .test_config import config


@pytest.fixture
async def auth_server():
    calls = []

    async def authenticate(request):
        calls.append(dict(await request.post()))
        return web.json_response(
            {"access_token": "test-token", "expire_at": time.time() + 3600}
        )

    app = web.Application()
    app.router.add_post("/v1/authenticate", authenticate)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    port = runner.addresses[0][1]
    yield f"http://127.0.0.1:{port}", calls
    await runner.cleanup()


async def test_auth_form_cache_refresh_and_close(auth_server):
    base, calls = auth_server
    client = RTZRClient(config(api_base=base))
    try:
        assert (
            await asyncio.gather(client.token(), client.token())
            == ["test-token"] * 2
        )
        assert calls == [{"client_id": "id", "client_secret": "secret"}]
        client._expire_at = time.time() + 20
        assert await client.token() == "test-token"
        assert len(calls) == 2
        session = client.session
    finally:
        await client.close()
    assert session.closed


async def test_connect_query_excludes_credentials(auth_server):
    base, _ = auth_server
    client = RTZRClient(config(api_base=base, use_itn=False))
    try:
        session = client._session()
        session.ws_connect = AsyncMock(return_value="socket")
        assert await client.connect() == "socket"
        call = session.ws_connect.call_args
        assert call.kwargs["headers"] == {"Authorization": "Bearer test-token"}
        assert call.kwargs["params"]["use_itn"] == "false"
        assert "client_id" not in call.kwargs["params"]
    finally:
        await client.close()
