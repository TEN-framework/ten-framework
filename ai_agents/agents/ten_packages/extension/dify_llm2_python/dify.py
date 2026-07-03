# ------------------------------
# Config
# ------------------------------
from dataclasses import dataclass
import json
from typing import AsyncGenerator, Optional

import aiohttp
from pydantic import BaseModel
from ten_ai_base.struct import (
    LLMMessageContent,
    LLMRequest,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
)
from ten_runtime import AsyncTenEnv


@dataclass
class DifyLLM2Config(BaseModel):
    api_key: str = ""
    base_url: str = "https://api.dify.ai/v1"
    user_id: str = "TenAgent"
    # Networking
    connect_timeout_s: float = 15.0
    total_timeout_s: float = 60.0
    # Provider specific additions (ignored by Dify)


# ------------------------------
# Thin Dify streaming client
# ------------------------------
class DifyChatClient:
    def __init__(self, ten_env: AsyncTenEnv, config: DifyLLM2Config):
        self.ten_env = ten_env
        self.config = config
        self._session: Optional[aiohttp.ClientSession] = None
        self._conversation_id: str = ""

    async def _ensure_session(self):
        if self._session is None or self._session.closed:
            timeout = aiohttp.ClientTimeout(
                connect=self.config.connect_timeout_s,
                total=self.config.total_timeout_s,
            )
            self._session = aiohttp.ClientSession(timeout=timeout)

    async def aclose(self):
        if self._session and not self._session.closed:
            await self._session.close()
        self._session = None

    def _headers(self):
        return {
            "Authorization": f"Bearer {self.config.api_key}",
            "Content-Type": "application/json",
        }

    def _url(self, path: str) -> str:
        base = self.config.base_url.rstrip("/")
        return f"{base}/{path.lstrip('/')}"

    async def get_chat_completions(
        self, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        """
        Map LLMRequest -> Dify /chat-messages streaming API.
        Emit LLMResponseMessageDelta and LLMResponseMessageDone, mirroring the OpenAI LLM2 sample.
        """
        await self._ensure_session()
        assert self._session is not None

        # Dify takes a single "query" string. We choose the latest user message text for parity with your old code.
        query_text = ""
        for m in reversed(request_input.messages or []):
            if isinstance(m, LLMMessageContent) and m.role == "user":
                if isinstance(m.content, str):
                    query_text = m.content
                    break
                if isinstance(m.content, list):
                    # Flatten simple text chunks if present
                    text_chunks = [
                        getattr(x, "text", "")
                        for x in m.content
                        if hasattr(x, "text")
                    ]
                    query_text = "\n".join([t for t in text_chunks if t])
                    break

        if not query_text:
            # As a fallback, take the very last text-looking message
            for m in reversed(request_input.messages or []):
                if isinstance(m, LLMMessageContent) and isinstance(
                    m.content, str
                ):
                    query_text = m.content
                    break

        # NOTE: Dify does not support tool calls in this endpoint; we ignore tools/messages of function types.
        # Keep behavior symmetrical with your OpenAI extension: we only stream assistant text.

        payload = {
            "inputs": {},
            "query": query_text,
            "response_mode": "streaming",
        }
        if self._conversation_id:
            payload["conversation_id"] = self._conversation_id
        if self.config.user_id:
            payload["user"] = self.config.user_id

        self.ten_env.log_info(
            f"[Dify] POST {self._url('chat-messages')} payload={payload}"
        )

        full_content = ""
        async with self._session.post(
            self._url("chat-messages"), json=payload, headers=self._headers()
        ) as resp:
            if resp.status != 200:
                try:
                    err = await resp.json()
                except Exception:
                    err = {"status": resp.status, "text": await resp.text()}
                raise RuntimeError(f"Dify chat-messages failed: {err}")

            async for raw in resp.content:
                if not raw:
                    continue
                line = raw.decode("utf-8").strip()
                if not line.startswith("data:"):
                    continue

                content = line[5:].strip()
                if content == "[DONE]":
                    # Close event: send MessageDone
                    break

                # Each line is a JSON object like:
                # {"event":"message","id":"...","task_id":"...","answer":"...","conversation_id":"...","created_at":1705398420}
                try:
                    evt = json.loads(content)
                except Exception:
                    continue

                event_type = evt.get("event")
                if event_type in ("message", "agent_message"):
                    # cache conversation id once
                    if not self._conversation_id and evt.get("conversation_id"):
                        self._conversation_id = evt["conversation_id"]
                        self.ten_env.log_info(
                            f"[Dify] conversation_id={self._conversation_id}"
                        )

                    delta = evt.get("answer") or ""
                    if not delta:
                        continue
                    full_content += delta

                    # Stream assistant delta
                    yield LLMResponseMessageDelta(
                        response_id=str(evt.get("id") or ""),
                        role="assistant",
                        content=full_content,
                        delta=delta,
                        created=int(evt.get("created_at") or 0),
                    )

                elif event_type == "message_end":
                    # Can log metadata; final "DONE" still closes the stream
                    meta = evt.get("metadata", {})
                    self.ten_env.log_debug(
                        f"[Dify] message_end metadata={meta}"
                    )

                elif event_type == "error":
                    msg = evt.get("message") or "unknown provider error"
                    raise RuntimeError(f"Dify stream error: {msg}")

        # Emit the terminal message (even if empty) to mirror OpenAI sample
        yield LLMResponseMessageDone(
            response_id="",
            role="assistant",
            content=full_content,
            created=0,
        )
