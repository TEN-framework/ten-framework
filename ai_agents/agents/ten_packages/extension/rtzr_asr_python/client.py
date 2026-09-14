import asyncio
import time

import aiohttp

from .config import RTZRASRConfig


class RTZRClient:
    """Own the HTTP session and reusable token for one TEN extension."""

    def __init__(self, config: RTZRASRConfig):
        self.config = config
        self.session: aiohttp.ClientSession | None = None
        self._token = ""
        self._expire_at = 0.0
        self._token_lock = asyncio.Lock()

    def _session(self) -> aiohttp.ClientSession:
        if self.session is None or self.session.closed:
            self.session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=30)
            )
        return self.session

    async def token(self) -> str:
        async with self._token_lock:
            if self._token and time.time() + 300 < self._expire_at:
                return self._token
            params = self.config.params
            async with self._session().post(
                params["api_base"] + "/v1/authenticate",
                data={
                    key: params[key] for key in ("client_id", "client_secret")
                },
                allow_redirects=False,
            ) as response:
                response.raise_for_status()
                payload = await response.json()
            token = payload.get("access_token")
            expiry = payload.get("expire_at")
            if (
                not isinstance(token, str)
                or not token
                or type(expiry) not in (int, float)
                or expiry <= time.time()
            ):
                raise ValueError("invalid RTZR authentication response")
            self._token, self._expire_at = token, expiry
            return token

    async def connect(self) -> aiohttp.ClientWebSocketResponse:
        for attempt in range(2):
            # Authentication endpoint failures remain terminal. Only a
            # WebSocket 401 gets one attempt with a freshly issued token.
            token = await self.token()
            try:
                return await asyncio.wait_for(
                    self._session().ws_connect(
                        self.config.params["websocket_url"]
                        + "/v1/transcribe:streaming",
                        params=self.config.query_params(),
                        headers={"Authorization": f"Bearer {token}"},
                        heartbeat=15,
                    ),
                    timeout=30,
                )
            except aiohttp.ClientResponseError as exc:
                if exc.status != 401:
                    raise
                self._token = ""
                if attempt:
                    raise

    async def close(self) -> None:
        if self.session is not None:
            await self.session.close()
            self.session = None
