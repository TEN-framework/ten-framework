"""Blaze realtime STT extension for the TEN framework.

Implements the TEN ASR extension interface (``AsyncASRBaseExtension``) on top of
the Blaze realtime STT websocket endpoint. Transcripts are emitted as
``asr_result`` data and failures (invalid API key, connection errors, server
errors) are surfaced as ``error`` data via ``send_asr_error``.

Protocol (WebSocket: /v1/stt/realtime):
    1. Connect to ws(s)://<host>/v1/stt/realtime
    2. Send a JSON init message: {"token", "language", "model", "enable_log"}
    3. Receive {"type": "ready"} once the upstream session is established
    4. Stream binary PCM audio chunks (16 kHz, mono, 16-bit little-endian)
    5. Receive {"type": "partial"|"final"|"error", "text": "..."} messages
"""

import asyncio
import json

import websockets
from typing_extensions import override

from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)
from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)

from .blaze_stt_realtime import BlazeRealtimeClient
from .config import BlazeSTTRealtimeConfig
from .const import MODULE_NAME_ASR


class BlazeSTTRealtimeExtension(AsyncASRBaseExtension):
    """Blaze realtime (streaming) Speech-to-Text extension."""

    def __init__(self, name: str):
        super().__init__(name)
        self.config: BlazeSTTRealtimeConfig | None = None
        self.ws_url: str = ""
        self.websocket = None
        self.connected: bool = False
        self._recv_task: asyncio.Task[None] | None = None

    @override
    def vendor(self) -> str:
        return "blaze"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = BlazeSTTRealtimeConfig.model_validate_json(
                config_json
            )
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
        except Exception as e:
            ten_env.log_error(f"Invalid Blaze realtime STT config: {e}")
            self.config = BlazeSTTRealtimeConfig()
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        """Open the realtime websocket session and complete the handshake."""
        assert self.config is not None
        self.ten_env.log_info("Starting Blaze realtime STT connection")

        # Validate credentials up front so a missing key is a clear fatal error.
        api_key = self.config.api_key
        if not api_key or not str(api_key).strip():
            error_msg = "Blaze STT API key is required but missing or empty"
            self.ten_env.log_error(error_msg)
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=error_msg,
                ),
            )
            return

        # Drop any existing connection before reconnecting.
        if self.is_connected():
            await self.stop_connection()

        self.ws_url = BlazeRealtimeClient.build_ws_url(
            self.config.api_url.rstrip("/")
        )

        init_message = {
            "token": api_key,
            "language": self.config.language,
            "model": self.config.model,
            "enable_log": self.config.enable_log,
        }

        try:
            self.websocket = await websockets.connect(
                self.ws_url,
                max_size=None,
                open_timeout=self.config.timeout,
            )

            await self.websocket.send(json.dumps(init_message))

            # Wait for the server "ready" signal before streaming audio, bounded
            # so a silent/hung server can't block the connection indefinitely.
            try:
                await asyncio.wait_for(
                    self._await_ready(),
                    timeout=self.config.handshake_timeout,
                )
            except asyncio.TimeoutError as e:
                raise RuntimeError(
                    "Timed out waiting for realtime STT 'ready' after "
                    f"{self.config.handshake_timeout}s"
                ) from e

            self.connected = True
            self._recv_task = asyncio.create_task(self._recv_loop())

            self.ten_env.log_info(
                "vendor_status_changed: connected",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            self.ten_env.log_error(
                f"Failed to start Blaze realtime STT connection: {e}"
            )
            await self._close_websocket()
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="",
                    message=str(e),
                ),
            )

    async def _await_ready(self) -> None:
        """Block until the server sends a 'ready' message.

        Raises RuntimeError if the server reports an error before becoming
        ready. Pre-ready binary frames and unknown message types are ignored.
        """
        assert self.websocket is not None
        while True:
            raw = await self.websocket.recv()
            if isinstance(raw, bytes):
                continue
            msg = json.loads(raw)
            msg_type = msg.get("type")
            if msg_type == "ready":
                return
            if msg_type == "error":
                raise RuntimeError(
                    msg.get("text") or "Realtime STT error before ready"
                )
            # Ignore any other pre-ready chatter.

    async def _recv_loop(self) -> None:
        """Consume transcript messages until the connection closes."""
        assert self.websocket is not None
        try:
            async for raw in self.websocket:
                if isinstance(raw, bytes):
                    continue
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    self.ten_env.log_warn(f"Non-JSON message ignored: {raw!r}")
                    continue

                msg_type = msg.get("type")
                if msg_type in ("partial", "final"):
                    await self._emit_result(msg, final=msg_type == "final")
                elif msg_type == "error":
                    await self._emit_error(msg)
                # "ready" / unknown types after handshake are ignored.
        except websockets.ConnectionClosed:
            self.ten_env.log_info("Blaze realtime STT connection closed")
        except Exception as e:  # noqa: BLE001 - log and surface as error
            self.ten_env.log_error(f"Blaze realtime STT recv loop error: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=str(e),
                ),
            )
        finally:
            self.connected = False

    async def _emit_result(self, msg: dict, final: bool) -> None:
        text = msg.get("text") or ""
        asr_result = ASRResult(
            text=text,
            final=final,
            start_ms=0,
            duration_ms=0,
            language=self.config.language if self.config else "",
            words=[],
        )
        await self.send_asr_result(asr_result)

    async def _emit_error(self, msg: dict) -> None:
        message = msg.get("text") or "Realtime STT error"
        self.ten_env.log_error(
            f"vendor_error: {message}", category=LOG_CATEGORY_VENDOR
        )
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=message,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code="",
                message=message,
            ),
        )

    @override
    def is_connected(self) -> bool:
        return self.connected and self.websocket is not None

    @override
    async def stop_connection(self) -> None:
        await self._close_websocket()

    async def _close_websocket(self) -> None:
        self.connected = False

        if self._recv_task is not None and not self._recv_task.done():
            self._recv_task.cancel()
            try:
                await self._recv_task
            except asyncio.CancelledError:
                pass
        self._recv_task = None

        if self.websocket is not None:
            try:
                await self.websocket.close()
            except Exception as e:  # noqa: BLE001
                self.ten_env.log_warn(f"Error closing websocket: {e}")
            self.websocket = None

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        # Keep audio buffered (up to 10 MB) until the session is connected.
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        if not self.is_connected():
            return False

        buf = frame.lock_buf()
        try:
            await self.websocket.send(bytes(buf))
            return True
        except Exception as e:  # noqa: BLE001
            self.ten_env.log_error(f"Error sending audio to Blaze: {e}")
            return False
        finally:
            frame.unlock_buf(buf)

    @override
    async def finalize(self, _session_id: str | None) -> None:
        """Signal end-of-turn.

        The Blaze realtime protocol has no explicit finalize frame; trailing
        finals arrive over the open connection. We simply report the finalize
        as complete so the pipeline's TTLW metric is recorded.
        """
        self.ten_env.log_debug("Blaze realtime STT finalize")
        await self.send_asr_finalize_end()
