import asyncio
import json
from typing import Any, AsyncGenerator
from urllib.parse import urlencode, urlparse, urlsplit, urlunsplit

import aiohttp

from ten_runtime import AsyncTenEnv

from .struct import BinaryAudioMessage, parse_server_message, to_json


def smart_str(s: str, max_field_len: int = 128) -> str:
    try:
        data = json.loads(s)
        for key in ("secret_key", "text", "content", "message"):
            value = data.get(key)
            if isinstance(value, str) and len(value) > max_field_len:
                data[key] = value[:max_field_len] + "..."
        return json.dumps(data)
    except json.JSONDecodeError:
        return s


class FPTTokenManager:
    def __init__(
        self,
        ten_env: AsyncTenEnv,
        token_url: str,
        api_key: str,
        agent_id: str,
        agent_type: str,
    ):
        self.ten_env = ten_env
        self.token_url = token_url
        self.api_key = api_key
        self.agent_id = agent_id
        self.agent_type = agent_type
        self.token = ""
        self.expires_at = 0.0
        self.session = aiohttp.ClientSession()

    async def close(self) -> None:
        await self.session.close()

    async def get_token(self, force_refresh: bool = False) -> str:
        now = asyncio.get_running_loop().time()
        if (
            not force_refresh
            and self.token
            and self.expires_at
            and now < self.expires_at - 30
        ):
            return self.token

        headers = {"x-api-key": self.api_key}
        async with self.session.get(
            self._token_request_url(), headers=headers
        ) as response:
            response.raise_for_status()
            data = await response.json()

        token = data.get("token", "")
        if not isinstance(token, str) or not token:
            raise ValueError("missing 'token' in token response")

        expires_in = data.get("expires_in", 0)
        try:
            expires_in_int = int(expires_in)
        except (TypeError, ValueError):
            expires_in_int = 0

        self.token = token
        self.expires_at = now + max(expires_in_int, 0)
        return token

    def _token_request_url(self) -> str:
        parts = urlsplit(self.token_url)
        query = urlencode(
            {
                "agent_type": self.agent_type,
                "agent_id": self.agent_id,
            }
        )
        return urlunsplit(
            (parts.scheme, parts.netloc, parts.path, query, parts.fragment)
        )


class RealtimeApiConnection:
    def __init__(
        self,
        ten_env: AsyncTenEnv,
        websocket_url: str,
        verbose: bool = False,
    ):
        self.ten_env = ten_env
        self.websocket_url = websocket_url
        self.websocket: aiohttp.ClientWebSocketResponse | None = None
        self.verbose = verbose
        self.session = aiohttp.ClientSession()

    async def connect(self) -> None:
        self.websocket = await self.session.ws_connect(self.websocket_url)
        parsed = urlparse(self.websocket_url)
        self.ten_env.log_info(
            "FPT websocket connected: "
            f"scheme={parsed.scheme}, host={parsed.netloc}, path={parsed.path}"
        )

    async def send_auth(
        self,
        token: str,
        agent_id: str,
        agent_type: str,
        voice: str,
        voice_speed: float,
    ) -> None:
        await self.send_json(
            {
                "type": "auth",
                "secret_key": token,
                "bot_info": {"id": agent_id, "type": agent_type},
                "user_attributes": {},
                "voice": voice,
                "voice_speed": voice_speed,
                "client_status": {},
            }
        )

    async def send_bridge_connect(self, call_id: str) -> None:
        await self.send_json({"type": "bridge_connect", "call_id": call_id})

    async def send_bridge_disconnect(self) -> None:
        await self.send_json({"type": "bridge_disconnect"})

    async def send_audio_data(self, audio_data: bytes) -> None:
        assert self.websocket is not None
        await self.websocket.send_bytes(audio_data)

    async def send_json(self, message: Any) -> None:
        assert self.websocket is not None
        payload = to_json(message)
        if self.verbose:
            self.ten_env.log_info(f"-> {smart_str(payload)}")
        await self.websocket.send_str(payload)

    async def listen(self) -> AsyncGenerator[Any, None]:
        assert self.websocket is not None
        try:
            async for msg in self.websocket:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    if self.verbose:
                        self.ten_env.log_info(f"<- {smart_str(msg.data)}")
                    yield parse_server_message(msg.data)
                elif msg.type == aiohttp.WSMsgType.BINARY:
                    yield BinaryAudioMessage(audio=bytes(msg.data))
                elif msg.type == aiohttp.WSMsgType.ERROR:
                    exc = self.websocket.exception()
                    if exc:
                        raise exc
                    break
            raise ConnectionError(
                "FPT websocket stream ended: "
                f"close_code={self.websocket.close_code}, "
                f"exception={self.websocket.exception()}"
            )
        except asyncio.CancelledError:
            self.ten_env.log_info("FPT websocket listener cancelled")

    async def close(self) -> None:
        if self.websocket is not None:
            await self.websocket.close()
            self.websocket = None
        await self.session.close()
