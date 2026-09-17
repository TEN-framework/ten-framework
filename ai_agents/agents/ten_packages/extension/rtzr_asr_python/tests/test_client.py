import asyncio
import time
from unittest.mock import AsyncMock

import aiohttp
import pytest
from aiohttp import web
from ten_packages.extension.rtzr_asr_python.client import RTZRClient

from .test_config import config


@pytest.fixture
async def auth_server():
    calls = []

    async def authenticate(request):
        calls.append(dict(await request.post()))
        if calls[-1]["client_secret"] == "invalid":
            raise web.HTTPUnauthorized()
        return web.json_response(
            {"access_token": "test-token", "expire_at": time.time() + 3600}
        )

    async def streaming(request):
        if request.headers.get("Authorization") != "Bearer test-token":
            raise web.HTTPUnauthorized()
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        async for _message in ws:
            pass
        return ws

    app = web.Application()
    app.router.add_post("/v1/authenticate", authenticate)
    app.router.add_get("/v1/transcribe:streaming", streaming)
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


async def test_revoked_cached_token_refreshes_and_connects(auth_server):
    base, calls = auth_server
    client = RTZRClient(config(api_base=base))
    client._token = "revoked-token"
    client._expire_at = time.time() + 3600
    try:
        ws = await client.connect()
        assert not ws.closed
        assert len(calls) == 1
        assert client._token == "test-token"
        await ws.close()
    finally:
        await client.close()


@pytest.mark.parametrize("status,attempts", [(401, 2), (403, 1)])
async def test_handshake_auth_failure_is_bounded(auth_server, status, attempts):
    base, calls = auth_server
    client = RTZRClient(config(api_base=base))
    try:
        connect = AsyncMock(
            side_effect=aiohttp.ClientResponseError(None, (), status=status)
        )
        client._session().ws_connect = connect
        with pytest.raises(aiohttp.ClientResponseError) as error:
            await client.connect()
        assert error.value.status == status
        assert connect.await_count == attempts
        assert len(calls) == attempts
    finally:
        await client.close()


async def test_authentication_401_is_not_retried(auth_server):
    base, calls = auth_server
    client = RTZRClient(config(api_base=base, client_secret="invalid"))
    try:
        with pytest.raises(aiohttp.ClientResponseError) as error:
            await client.connect()
        assert error.value.status == 401
        assert len(calls) == 1
    finally:
        await client.close()
