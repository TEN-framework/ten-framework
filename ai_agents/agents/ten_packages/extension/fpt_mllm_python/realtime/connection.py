import asyncio
import base64
import json
import time
from typing import Any, AsyncGenerator

import aiohttp

from ten_runtime import AsyncTenEnv

from .struct import (
    InputAudioBufferAppend,
    parse_server_message,
    to_json,
)


def smart_str(s: str, max_field_len: int = 128) -> str:
    try:
        data = json.loads(s)
        for key in ("delta", "audio", "token"):
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
        app_id: str,
    ):
        self.ten_env = ten_env
        self.token_url = token_url
        self.api_key = api_key
        self.app_id = app_id
        self.token = ""
        self.expires_at = 0.0
        self.session = aiohttp.ClientSession()

    async def close(self) -> None:
        await self.session.close()

    async def get_token(self, force_refresh: bool = False) -> str:
        now = time.time()
        if (
            not force_refresh
            and self.token
            and self.expires_at
            and now < self.expires_at - 30
        ):
            return self.token

        payload: dict[str, Any] = {"api_key": self.api_key}
        if self.app_id:
            payload["app_id"] = self.app_id

        async with self.session.post(self.token_url, json=payload) as response:
            response.raise_for_status()
            data = await response.json()

        token = data.get("access_token", "")
        if not isinstance(token, str) or not token:
            raise ValueError("missing 'access_token' in token response")

        expires_in = data.get("expires_in", 0)
        try:
            expires_in_int = int(expires_in)
        except (TypeError, ValueError):
            expires_in_int = 0

        self.token = token
        self.expires_at = now + max(expires_in_int, 0)
        return token


class RealtimeApiConnection:
    def __init__(
        self,
        ten_env: AsyncTenEnv,
        websocket_url: str,
        token: str,
        verbose: bool = False,
    ):
        self.ten_env = ten_env
        self.websocket_url = websocket_url
        self.token = token
        self.websocket: aiohttp.ClientWebSocketResponse | None = None
        self.verbose = verbose
        self.session = aiohttp.ClientSession()

    async def connect(self) -> None:
        self.websocket = await self.session.ws_connect(
            self.websocket_url,
            headers={"Authorization": f"Bearer {self.token}"},
        )

    async def send_audio_data(self, audio_data: bytes) -> None:
        base64_audio_data = base64.b64encode(audio_data).decode("utf-8")
        await self.send_request(InputAudioBufferAppend(audio=base64_audio_data))

    async def send_request(self, message: Any) -> None:
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
                elif msg.type == aiohttp.WSMsgType.ERROR:
                    exc = self.websocket.exception()
                    if exc:
                        raise exc
                    break
        except asyncio.CancelledError:
            self.ten_env.log_info("FPT websocket listener cancelled")

    async def close(self) -> None:
        if self.websocket is not None:
            await self.websocket.close()
            self.websocket = None
        await self.session.close()
