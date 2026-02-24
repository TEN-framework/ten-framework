import asyncio
import base64
import hashlib
import json
import os
import time
import uuid
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Coroutine

import aiohttp
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey,
    Ed25519PublicKey,
)

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
DEFAULT_CLIENT_ID = "openclaw-control-ui"
DEFAULT_CLIENT_MODE = "ui"


@dataclass
class OpenclawConfig(BaseConfig):
    gateway_url: str = "ws://127.0.0.1:18789"
    gateway_token: str = ""
    gateway_password: str = ""
    gateway_scopes: str = ""
    gateway_client_id: str = DEFAULT_CLIENT_ID
    gateway_client_mode: str = DEFAULT_CLIENT_MODE
    gateway_origin: str = ""
    gateway_device_identity_path: str = "~/.openclaw/identity/device.json"
    chat_session_key: str = "agent:main:main"
    request_timeout_ms: int = 180000
    connect_timeout_ms: int = 10000


@dataclass
class PendingToolTask:
    task_id: str
    summary: str
    created_at_ms: int


@dataclass
class DeviceIdentity:
    device_id: str
    public_key_raw_b64url: str
    private_key: Ed25519PrivateKey


class GatewayRequestError(RuntimeError):
    def __init__(
        self,
        message: str,
        *,
        code: str = "",
        details: dict[str, Any] | None = None,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.details = details or {}


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
        self._connect_nonce = ""
        self._stopped = False
        self._device_identity: DeviceIdentity | None = None
        self._background_tasks: set[asyncio.Task] = set()
        self._last_pairing_emit_key = ""
        self._last_pairing_emit_at = 0.0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        self.config = await OpenclawConfig.create_async(ten_env=ten_env)
        self._device_identity = self._load_or_create_device_identity()
        self.session = aiohttp.ClientSession()
        await super().on_start(ten_env)
        await self._check_gateway_ready_on_start()

    async def on_stop(self, _ten_env: AsyncTenEnv) -> None:
        self._stopped = True
        for task in list(self._background_tasks):
            task.cancel()
        self._background_tasks.clear()
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
            if await self._emit_pairing_required_event(
                exc,
                task_id=task_id,
                summary=summary,
            ):
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content="OpenClaw pairing is required. I sent approval steps to the chat.",
                )
            return LLMToolResultLLMResult(
                type="llmresult",
                content="Failed to send task to OpenClaw.",
            )

    async def _check_gateway_ready_on_start(self) -> None:
        try:
            await self._ensure_connected()
            self.ten_env.log_info(
                "[openclaw_gateway_tool_python] gateway handshake ready on startup"
            )
        except Exception as exc:
            self.ten_env.log_warn(
                f"[openclaw_gateway_tool_python] startup handshake failed: {exc}"
            )
            emitted = await self._emit_pairing_required_event(
                exc,
                task_id="",
                summary="",
            )
            if emitted:
                self._create_background_task(
                    self._reemit_startup_pairing_notice(
                        exc=exc, task_id="", summary="", attempts=2, interval_seconds=1.0
                    )
                )
            else:
                await self._emit_reply_event(
                    {
                        "task_id": "",
                        "summary": "",
                        "reply_text": "",
                        "reply_ts": int(time.time() * 1000),
                        "error": f"OpenClaw startup handshake failed: {self._describe_connect_error(exc)}",
                    }
                )

    async def _emit_pairing_required_event(
        self,
        exc: Exception,
        *,
        task_id: str,
        summary: str,
    ) -> bool:
        pairing_payload = self._build_pairing_required_payload(exc)
        if not pairing_payload:
            return False
        pairing_payload.update(
            {
                "task_id": task_id,
                "summary": summary,
                "reply_text": "",
                "reply_ts": int(time.time() * 1000),
                "error": self._describe_connect_error(exc),
            }
        )
        if self._is_duplicate_pairing_event(pairing_payload):
            self.ten_env.log_info(
                "[openclaw_gateway_tool_python] skip duplicate pairing_required event"
            )
            return True
        await self._emit_reply_event(pairing_payload)
        return True

    async def _reemit_startup_pairing_notice(
        self,
        *,
        exc: Exception,
        task_id: str,
        summary: str,
        attempts: int,
        interval_seconds: float,
    ) -> None:
        for _ in range(max(attempts, 0)):
            if self._stopped:
                return
            await asyncio.sleep(max(interval_seconds, 0.1))
            await self._emit_pairing_required_event(
                exc,
                task_id=task_id,
                summary=summary,
            )

    async def _ensure_connected(self) -> None:
        async with self._connect_lock:
            if self._stopped:
                raise RuntimeError("extension stopped")
            if self.ws is None or self.ws.closed:
                await self._open_connection()
            timeout_s = max(self.config.connect_timeout_ms, 1000) / 1000
            deadline = time.monotonic() + timeout_s
            while not self._hello_event.is_set():
                if self.ws is None or self.ws.closed:
                    raise RuntimeError(self._describe_ws_state())
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise RuntimeError(
                        f"connect handshake timeout after {int(timeout_s * 1000)}ms; {self._describe_ws_state()}"
                    )
                try:
                    await asyncio.wait_for(
                        self._hello_event.wait(),
                        timeout=min(0.2, remaining),
                    )
                except asyncio.TimeoutError:
                    continue

    async def _open_connection(self) -> None:
        if self.session is None:
            raise RuntimeError("http session not initialized")
        self._hello_event.clear()
        self._connect_sent = False
        self._connect_nonce = ""
        headers: dict[str, str] = {}
        origin = str(self.config.gateway_origin or "").strip()
        if origin:
            headers["Origin"] = origin
        self.ws = await self.session.ws_connect(
            self.config.gateway_url,
            headers=headers if headers else None,
        )
        self.recv_task = asyncio.create_task(self._recv_loop())

    async def _recv_loop(self) -> None:
        assert self.ws is not None
        try:
            async for msg in self.ws:
                if msg.type != aiohttp.WSMsgType.TEXT:
                    continue
                await self._handle_message(str(msg.data))
        except Exception as exc:
            self.ten_env.log_warn(
                f"OpenClaw receive loop ended with error: {self._describe_connect_error(exc)}"
            )
        finally:
            self.ten_env.log_warn(
                f"OpenClaw receive loop closed: {self._describe_ws_state()}"
            )
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
                self._connect_nonce = self._extract_connect_nonce(payload)
                self._create_background_task(self._send_connect_background())
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
                error = frame.get("error", {}) or {}
                message = str(error.get("message", "request failed"))
                code = str(error.get("code", ""))
                details = (
                    error.get("details")
                    if isinstance(error.get("details"), dict)
                    else {}
                )
                fut.set_exception(
                    GatewayRequestError(
                        message,
                        code=code,
                        details=details,
                    )
                )

        if frame_type == "hello-ok":
            self._hello_event.set()

    async def _send_connect_background(self) -> None:
        try:
            await self._send_connect()
        except Exception as exc:
            self.ten_env.log_warn(
                f"[openclaw_gateway_tool_python] connect request failed: {self._describe_connect_error(exc)}"
            )
            await self._emit_pairing_required_event(
                exc,
                task_id="",
                summary="",
            )

    def _create_background_task(
        self, coro: Coroutine[Any, Any, Any]
    ) -> None:
        task = asyncio.create_task(coro)
        self._background_tasks.add(task)
        task.add_done_callback(self._background_tasks.discard)

    async def _send_connect(self) -> None:
        if self._connect_sent:
            return
        nonce = self._connect_nonce.strip()
        if not nonce:
            raise RuntimeError("gateway connect challenge missing nonce")
        self._connect_sent = True
        scopes = [
            s.strip()
            for s in str(self.config.gateway_scopes or "").split(",")
            if s.strip()
        ]
        client_id = str(self.config.gateway_client_id or "").strip()
        client_mode = str(self.config.gateway_client_mode or "").strip()
        if not client_id:
            client_id = DEFAULT_CLIENT_ID
        if not client_mode:
            client_mode = DEFAULT_CLIENT_MODE
        self.ten_env.log_info(
            f"[openclaw_gateway_tool_python] connect with client.id={client_id}, client.mode={client_mode}"
        )

        auth_token = str(self.config.gateway_token or "").strip()
        auth_password = str(self.config.gateway_password or "").strip()
        signed_at_ms = int(time.time() * 1000)
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
            "device": self._build_device_payload(
                client_id=client_id,
                client_mode=client_mode,
                scopes=scopes,
                signed_at_ms=signed_at_ms,
                token=auth_token,
                nonce=nonce,
            ),
        }
        if auth_token:
            payload["auth"] = {"token": auth_token}
        elif auth_password:
            payload["auth"] = {"password": auth_password}

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

    def _load_or_create_device_identity(self) -> DeviceIdentity:
        identity_path = Path(
            os.path.expanduser(
                str(self.config.gateway_device_identity_path or "").strip()
                or "~/.openclaw/identity/device.json"
            )
        )
        identity_path.parent.mkdir(parents=True, exist_ok=True)
        if identity_path.exists():
            loaded = self._load_device_identity(identity_path)
            if loaded:
                return loaded
            self.ten_env.log_warn(
                "[openclaw_gateway_tool_python] invalid device identity file, regenerating"
            )

        private_key = Ed25519PrivateKey.generate()
        public_key_raw = private_key.public_key().public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw,
        )
        device_id = hashlib.sha256(public_key_raw).hexdigest()
        public_key_pem = private_key.public_key().public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo,
        ).decode()
        private_key_pem = private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        ).decode()

        payload = {
            "version": 1,
            "device_id": device_id,
            "public_key_pem": public_key_pem,
            "private_key_pem": private_key_pem,
            "created_at_ms": int(time.time() * 1000),
        }
        identity_path.write_text(json.dumps(payload, indent=2) + "\n")
        try:
            os.chmod(identity_path, 0o600)
        except OSError:
            pass

        return DeviceIdentity(
            device_id=device_id,
            public_key_raw_b64url=self._base64url_encode(public_key_raw),
            private_key=private_key,
        )

    def _load_device_identity(
        self, identity_path: Path
    ) -> DeviceIdentity | None:
        try:
            payload = json.loads(identity_path.read_text())
            private_key_pem = str(payload.get("private_key_pem", "")).strip()
            public_key_pem = str(payload.get("public_key_pem", "")).strip()
            if not private_key_pem or not public_key_pem:
                return None

            private_key = serialization.load_pem_private_key(
                private_key_pem.encode(), password=None
            )
            if not isinstance(private_key, Ed25519PrivateKey):
                return None
            public_key = serialization.load_pem_public_key(
                public_key_pem.encode()
            )
            if not isinstance(public_key, Ed25519PublicKey):
                return None

            public_key_raw = public_key.public_bytes(
                encoding=serialization.Encoding.Raw,
                format=serialization.PublicFormat.Raw,
            )
            device_id = hashlib.sha256(public_key_raw).hexdigest()
            stored_device_id = str(payload.get("device_id", "")).strip()
            if stored_device_id and stored_device_id != device_id:
                self.ten_env.log_warn(
                    "[openclaw_gateway_tool_python] device_id mismatch in identity file; "
                    "using derived id from public key"
                )
            return DeviceIdentity(
                device_id=device_id,
                public_key_raw_b64url=self._base64url_encode(public_key_raw),
                private_key=private_key,
            )
        except Exception as exc:
            self.ten_env.log_warn(
                f"[openclaw_gateway_tool_python] failed to load device identity: {exc}"
            )
            return None

    def _build_device_payload(
        self,
        *,
        client_id: str,
        client_mode: str,
        scopes: list[str],
        signed_at_ms: int,
        token: str,
        nonce: str,
    ) -> dict[str, Any]:
        if self._device_identity is None:
            raise RuntimeError("device identity not initialized")
        device = self._device_identity
        signature_payload = self._build_device_auth_payload(
            device_id=device.device_id,
            client_id=client_id,
            client_mode=client_mode,
            role="operator",
            scopes=scopes,
            signed_at_ms=signed_at_ms,
            token=token,
            nonce=nonce,
        )
        signature = device.private_key.sign(signature_payload.encode("utf-8"))
        return {
            "id": device.device_id,
            "publicKey": device.public_key_raw_b64url,
            "signature": self._base64url_encode(signature),
            "signedAt": signed_at_ms,
            "nonce": nonce,
        }

    @staticmethod
    def _build_device_auth_payload(
        *,
        device_id: str,
        client_id: str,
        client_mode: str,
        role: str,
        scopes: list[str],
        signed_at_ms: int,
        token: str,
        nonce: str,
    ) -> str:
        return "|".join(
            [
                "v2",
                device_id,
                client_id,
                client_mode,
                role,
                ",".join(scopes),
                str(signed_at_ms),
                token or "",
                nonce,
            ]
        )

    @staticmethod
    def _extract_connect_nonce(payload: Any) -> str:
        if not isinstance(payload, dict):
            return ""
        nonce = payload.get("nonce")
        return str(nonce).strip() if isinstance(nonce, str) else ""

    @staticmethod
    def _base64url_encode(raw: bytes) -> str:
        return base64.urlsafe_b64encode(raw).decode().rstrip("=")

    def _build_pairing_required_payload(
        self, exc: Exception
    ) -> dict[str, Any] | None:
        message = self._describe_connect_error(exc)
        details: dict[str, Any] = {}
        code = ""
        if isinstance(exc, GatewayRequestError):
            details = exc.details
            code = str(exc.code or "").strip().lower()
        message_lower = message.lower()
        is_pairing_required = (
            "pairing required" in message_lower
            or "missing scope: operator.write" in message_lower
            or code == "pairing_required"
        )
        if not is_pairing_required:
            return None
        request_id = str(details.get("requestId", "")).strip()
        approve_cmd = (
            f"openclaw devices approve {request_id}"
            if request_id
            else "openclaw devices approve --latest"
        )
        return {
            "pairing_required": True,
            "pairing_list_cmd": "openclaw devices list",
            "pairing_approve_cmd": approve_cmd,
            "pairing_hint": "Run these commands on the gateway host, then restart this agent.",
        }

    def _is_duplicate_pairing_event(
        self, pairing_payload: dict[str, Any]
    ) -> bool:
        dedupe_payload = {
            "pairing_required": pairing_payload.get("pairing_required"),
            "pairing_list_cmd": pairing_payload.get("pairing_list_cmd"),
            "pairing_approve_cmd": pairing_payload.get("pairing_approve_cmd"),
            "pairing_hint": pairing_payload.get("pairing_hint"),
            "error": pairing_payload.get("error"),
        }
        event_key = json.dumps(dedupe_payload, sort_keys=True, ensure_ascii=True)
        now = time.monotonic()
        # Suppress bursts from concurrent startup/connect paths.
        if (
            event_key == self._last_pairing_emit_key
            and now - self._last_pairing_emit_at < 5.0
        ):
            return True
        self._last_pairing_emit_key = event_key
        self._last_pairing_emit_at = now
        return False

    def _describe_connect_error(self, exc: Exception) -> str:
        raw = str(exc).strip()
        if raw:
            return raw
        return f"{exc.__class__.__name__}; {self._describe_ws_state()}"

    def _describe_ws_state(self) -> str:
        if self.ws is None:
            return "ws_state=none"
        close_code = getattr(self.ws, "close_code", None)
        ws_exc = self.ws.exception()
        ws_exc_text = str(ws_exc).strip() if ws_exc else ""
        return (
            f"ws_state=closed:{bool(self.ws.closed)} "
            f"close_code={close_code} "
            f"nonce_received={bool(self._connect_nonce)} "
            f"ws_exception={ws_exc_text or 'none'}"
        )

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
