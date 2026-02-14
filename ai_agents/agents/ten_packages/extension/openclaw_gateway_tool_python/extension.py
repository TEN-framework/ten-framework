import asyncio
import json
import time
import uuid
from dataclasses import dataclass
from datetime import datetime
from typing import Any

import aiohttp

from ten_runtime import Data
from ten_runtime.async_ten_env import AsyncTenEnv
from ten_ai_base.config import BaseConfig
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
    LLMToolResultLLMResult,
)

TOOL_NAME = "claw_task_delegate"
DEFAULT_CLIENT_ID = "webchat-ui"
DEFAULT_CLIENT_MODE = "webchat"


@dataclass
class OpenclawConfig(BaseConfig):
    gateway_url: str = "ws://127.0.0.1:18789"
    gateway_token: str = ""
    gateway_password: str = ""
    gateway_scopes: str = ""
    gateway_client_id: str = DEFAULT_CLIENT_ID
    gateway_client_mode: str = DEFAULT_CLIENT_MODE
    gateway_origin: str = ""
    chat_session_key: str = "agent:main:main"
    request_timeout_ms: int = 180000
    connect_timeout_ms: int = 10000


@dataclass
class PendingToolTask:
    task_id: str
    summary: str
    created_at_ms: int


class OpenclawGatewayToolExtension(AsyncLLMToolBaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.config: OpenclawConfig | None = None
        self.session: aiohttp.ClientSession | None = None
        self.ws: aiohttp.ClientWebSocketResponse | None = None
        self.recv_task: asyncio.Task | None = None
        self._connect_lock = asyncio.Lock()
        self._hello_event = asyncio.Event()
        self._response_waiters: dict[str, asyncio.Future] = {}
        self._pending_tasks: dict[str, PendingToolTask] = {}
        self._connect_sent = False
        self._stopped = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        self.config = await OpenclawConfig.create_async(ten_env=ten_env)
        self.session = aiohttp.ClientSession()
        await super().on_start(ten_env)

    async def on_stop(self, _ten_env: AsyncTenEnv) -> None:
        self._stopped = True
        if self.recv_task:
            self.recv_task.cancel()
            try:
                await self.recv_task
            except asyncio.CancelledError:
                pass
            self.recv_task = None
        if self.ws is not None:
            await self.ws.close()
            self.ws = None
        if self.session is not None:
            await self.session.close()
            self.session = None
        self._hello_event.clear()
        for fut in self._response_waiters.values():
            if not fut.done():
                fut.set_exception(RuntimeError("gateway stopped"))
        self._response_waiters.clear()

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name=TOOL_NAME,
                description="Delegate complex tasks to OpenClaw. Provide a short summary of the task.",
                parameters=[
                    LLMToolMetadataParameter(
                        name="summary",
                        type="string",
                        description="Summary of the complex task to delegate.",
                        required=True,
                    )
                ],
            )
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        if name != TOOL_NAME:
            return None

        await self._prune_pending_tasks()

        summary = str(args.get("summary", "")).strip()
        if not summary:
            return LLMToolResultLLMResult(
                type="llmresult",
                content="OpenClaw task was not sent: summary is empty.",
            )

        task_id = str(uuid.uuid4())
        self._pending_tasks[task_id] = PendingToolTask(
            task_id=task_id,
            summary=summary,
            created_at_ms=int(time.time() * 1000),
        )

        try:
            await self._ensure_connected()
            await self._request(
                "chat.send",
                {
                    "sessionKey": self.config.chat_session_key,
                    "message": summary,
                    "deliver": False,
                    "idempotencyKey": task_id,
                },
                timeout_ms=self.config.request_timeout_ms,
            )
            return LLMToolResultLLMResult(
                type="llmresult",
                content="Task sent to OpenClaw. I will report back when complete.",
            )
        except Exception as exc:
            self._pending_tasks.pop(task_id, None)
            self.ten_env.log_error(f"Failed to send OpenClaw task: {exc}")
            return LLMToolResultLLMResult(
                type="llmresult",
                content="Failed to send task to OpenClaw.",
            )

    async def _ensure_connected(self) -> None:
        async with self._connect_lock:
            if self._stopped:
                raise RuntimeError("extension stopped")
            if self.ws is None or self.ws.closed:
                await self._open_connection()
            if not self._hello_event.is_set():
                await asyncio.wait_for(
                    self._hello_event.wait(),
                    timeout=max(self.config.connect_timeout_ms, 1000) / 1000,
                )

    async def _open_connection(self) -> None:
        if self.session is None:
            raise RuntimeError("http session not initialized")
        self._hello_event.clear()
        self._connect_sent = False
        headers: dict[str, str] = {}
        origin = str(self.config.gateway_origin or "").strip()
        if origin:
            headers["Origin"] = origin
        self.ws = await self.session.ws_connect(
            self.config.gateway_url,
            headers=headers if headers else None,
        )
        self.recv_task = asyncio.create_task(self._recv_loop())
        await self._send_connect()

    async def _recv_loop(self) -> None:
        assert self.ws is not None
        try:
            async for msg in self.ws:
                if msg.type != aiohttp.WSMsgType.TEXT:
                    continue
                await self._handle_message(str(msg.data))
        except Exception as exc:
            self.ten_env.log_warn(
                f"OpenClaw receive loop ended with error: {exc}"
            )
        finally:
            self._hello_event.clear()
            if self.ws is not None and not self.ws.closed:
                await self.ws.close()
            self.ws = None

    async def _handle_message(self, raw: str) -> None:
        try:
            frame = json.loads(raw)
        except json.JSONDecodeError:
            return

        frame_type = frame.get("type")
        if frame_type == "event":
            event_name = frame.get("event")
            payload = frame.get("payload")
            if event_name == "connect.challenge":
                await self._send_connect()
                return
            if event_name == "agent":
                phase = self._extract_agent_phase(payload)
                if phase:
                    await self._emit_reply_event(
                        {
                            "task_id": "",
                            "summary": "",
                            "reply_text": "",
                            "reply_ts": int(time.time() * 1000),
                            "agent_phase": phase,
                        }
                    )
                return
            if event_name == "chat":
                await self._handle_chat_event(payload)
                return

        if frame_type == "res":
            req_id = str(frame.get("id", ""))
            fut = self._response_waiters.pop(req_id, None)
            if fut is None or fut.done():
                return
            if frame.get("ok"):
                fut.set_result(frame.get("payload"))
            else:
                error = frame.get("error", {})
                fut.set_exception(
                    RuntimeError(str(error.get("message", "request failed")))
                )

        if frame_type == "hello-ok":
            self._hello_event.set()

    async def _send_connect(self) -> None:
        if self._connect_sent:
            return
        self._connect_sent = True
        scopes = [
            s.strip()
            for s in str(self.config.gateway_scopes or "").split(",")
            if s.strip()
        ]
        client_id = str(self.config.gateway_client_id or "").strip()
        client_mode = str(self.config.gateway_client_mode or "").strip()
        if client_id != DEFAULT_CLIENT_ID:
            self.ten_env.log_warn(
                f"[openclaw_gateway_tool_python] overriding unsupported client id `{client_id}` -> `{DEFAULT_CLIENT_ID}`"
            )
            client_id = DEFAULT_CLIENT_ID
        if client_mode != DEFAULT_CLIENT_MODE:
            self.ten_env.log_warn(
                f"[openclaw_gateway_tool_python] overriding unsupported client mode `{client_mode}` -> `{DEFAULT_CLIENT_MODE}`"
            )
            client_mode = DEFAULT_CLIENT_MODE

        payload = {
            "minProtocol": 3,
            "maxProtocol": 3,
            "client": {
                "id": client_id,
                "version": "0.1.0",
                "platform": "ten",
                "mode": client_mode,
            },
            "role": "operator",
            "scopes": scopes,
            "caps": [],
            "locale": "en-US",
        }
        if self.config.gateway_token:
            payload["auth"] = {"token": self.config.gateway_token}
        elif self.config.gateway_password:
            payload["auth"] = {"password": self.config.gateway_password}

        hello = await self._request(
            method="connect",
            params=payload,
            timeout_ms=self.config.connect_timeout_ms,
        )
        if isinstance(hello, dict) and hello.get("type") == "hello-ok":
            self._hello_event.set()

    async def _request(self, method: str, params: Any, timeout_ms: int) -> Any:
        if self.ws is None or self.ws.closed:
            raise RuntimeError("gateway not connected")

        req_id = str(uuid.uuid4())
        loop = asyncio.get_running_loop()
        fut = loop.create_future()
        self._response_waiters[req_id] = fut

        frame = {
            "type": "req",
            "id": req_id,
            "method": method,
            "params": params,
        }
        await self.ws.send_str(json.dumps(frame))

        timeout = max(timeout_ms, 1000) / 1000
        try:
            return await asyncio.wait_for(fut, timeout=timeout)
        finally:
            self._response_waiters.pop(req_id, None)

    async def _handle_chat_event(self, payload: Any) -> None:
        if not isinstance(payload, dict):
            return

        state = payload.get("state")
        if state and state != "final":
            return

        reply_text = self._extract_chat_message_text(payload.get("message"))
        if not reply_text:
            return

        reply_ts = self._extract_chat_timestamp(payload.get("message"))
        task = self._pop_pending_task()
        await self._emit_reply_event(
            {
                "task_id": task.task_id if task else "",
                "summary": task.summary if task else "",
                "reply_text": reply_text,
                "reply_ts": reply_ts or int(time.time() * 1000),
            }
        )

    def _pop_pending_task(self) -> PendingToolTask | None:
        if not self._pending_tasks:
            return None
        first_key = next(iter(self._pending_tasks.keys()))
        return self._pending_tasks.pop(first_key, None)

    async def _prune_pending_tasks(self) -> None:
        if not self._pending_tasks:
            return
        now_ms = int(time.time() * 1000)
        timeout_ms = max(self.config.request_timeout_ms, 1000)
        expired: list[PendingToolTask] = []
        for key, task in list(self._pending_tasks.items()):
            if now_ms - task.created_at_ms >= timeout_ms:
                expired.append(task)
                self._pending_tasks.pop(key, None)

        for task in expired:
            await self._emit_reply_event(
                {
                    "task_id": task.task_id,
                    "summary": task.summary,
                    "reply_text": "",
                    "reply_ts": now_ms,
                    "error": "OpenClaw request timed out",
                }
            )

    async def _emit_reply_event(self, payload: dict[str, Any]) -> None:
        data = Data.create("openclaw_reply_event")
        data.set_property_from_json(None, json.dumps(payload))
        await self.ten_env.send_data(data)

    @staticmethod
    def _extract_agent_phase(payload: Any) -> str:
        if not isinstance(payload, dict):
            return ""
        data = payload.get("data")
        if not isinstance(data, dict):
            return ""
        phase = str(data.get("phase", "")).strip()
        name = str(data.get("name", "")).strip()
        error = str(data.get("error", "")).strip()
        label_parts = [p for p in (phase, name) if p]
        label = " · ".join(label_parts)
        if error:
            return f"{label or 'error'}: {error}"
        return label

    @classmethod
    def _extract_chat_message_text(cls, message: Any) -> str:
        if isinstance(message, str):
            return message.strip()
        if not isinstance(message, dict):
            return ""

        nested = message.get("message")
        nested_text = cls._extract_chat_message_text(nested)
        if nested_text:
            return nested_text

        content = message.get("content")
        if isinstance(content, str):
            return content.strip()

        if isinstance(content, list):
            texts: list[str] = []
            for item in content:
                if not isinstance(item, dict):
                    continue
                if item.get("type") == "text" and isinstance(
                    item.get("text"), str
                ):
                    texts.append(item["text"])
            if texts:
                return "\n".join(texts).strip()

        if isinstance(message.get("text"), str):
            return str(message.get("text", "")).strip()

        return ""

    @staticmethod
    def _extract_chat_timestamp(message: Any) -> int | None:
        if not isinstance(message, dict):
            return None
        ts = message.get("timestamp")
        if isinstance(ts, int):
            return ts
        if isinstance(ts, float):
            return int(ts)
        if isinstance(ts, str):
            try:
                normalized = ts.replace("Z", "+00:00")
                return int(
                    datetime.fromisoformat(normalized).timestamp() * 1000
                )
            except Exception:
                return None
        return None
