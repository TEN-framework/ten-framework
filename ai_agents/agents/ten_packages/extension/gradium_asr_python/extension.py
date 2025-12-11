"""
Gradium ASR extension for TEN framework.
"""

import asyncio
import base64
import json

from typing import Any
from typing_extensions import override
import websockets

from .config import GradiumASRConfig
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT
from .const import (
    MODULE_NAME_ASR,
    WS_MSG_TYPE_SETUP,
    WS_MSG_TYPE_READY,
    WS_MSG_TYPE_AUDIO,
    WS_MSG_TYPE_TEXT,
    WS_MSG_TYPE_VAD,
    WS_MSG_TYPE_END,
    GRADIUM_SAMPLE_RATE,
)
from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeDiscard,
    AsyncASRBaseExtension,
)
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)
from ten_ai_base.helper import AsyncQueue


class GradiumASRExtension(AsyncASRBaseExtension):
    """Gradium ASR extension implementation."""

    def __init__(self, name: str):
        """
        Initialize the Gradium ASR extension.

        Args:
            name: Extension name.
        """
        super().__init__(name)
        self.config: GradiumASRConfig | None = None
        self.websocket: Any | None = None
        self.connected: bool = False
        self.receive_task: asyncio.Task | None = None
        self.audio_queue: AsyncQueue = AsyncQueue()
        self.send_task: asyncio.Task | None = None

    @override
    def vendor(self) -> str:
        """Get the ASR vendor name."""
        return "gradium"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """
        Initialize the extension.

        Args:
            ten_env: TEN environment.
        """
        await super().on_init(ten_env)

        # Load configuration
        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = GradiumASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = GradiumASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                )
            )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)

    @override
    async def start_connection(self) -> None:
        assert self.config is not None
        self.ten_env.log_info("start_connection")

        if self.connected:
            return

        try:
            # Connect to WebSocket
            self.websocket = await websockets.connect(
                self.config.get_websocket_url(),
                additional_headers={"x-api-key": self.config.api_key},
            )

            # Send setup message
            setup_message = {
                "type": WS_MSG_TYPE_SETUP,
                "model_name": self.config.model_name,
                "input_format": self.config.input_format,
            }

            if self.config.language:
                setup_message["language"] = self.config.language

            await self.websocket.send(json.dumps(setup_message))

            # Wait for ready message
            ready_msg = await self.websocket.recv()
            ready_data = json.loads(ready_msg)

            if ready_data.get("type") == WS_MSG_TYPE_READY:
                self.connected = True

                # Start receive task
                self.receive_task = asyncio.create_task(self._receive_loop())

                # Start send task
                self.send_task = asyncio.create_task(self._send_loop())
            else:
                raise ValueError(
                    f"Unexpected message type: {ready_data.get('type')}"
                )

        except Exception as e:
            self.connected = False
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="connection_error",
                    message=str(e),
                ),
            )

    @override
    async def stop_connection(self) -> None:
        if not self.connected:
            return

        try:
            # Send end of stream message
            if self.websocket:
                end_message = {"type": WS_MSG_TYPE_END}
                await self.websocket.send(json.dumps(end_message))

            # Cancel tasks
            if self.receive_task:
                self.receive_task.cancel()
                try:
                    await self.receive_task
                except asyncio.CancelledError:
                    pass

            if self.send_task:
                self.send_task.cancel()
                try:
                    await self.send_task
                except asyncio.CancelledError:
                    pass

            # Close WebSocket
            if self.websocket:
                await self.websocket.close()
                self.websocket = None

            self.connected = False

        except Exception:
            pass

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        if not self.connected:
            return False

        try:
            buf = frame.lock_buf()
            frame_bytes = bytes(buf)
            frame.unlock_buf(buf)

            # Queue audio for sending
            await self.audio_queue.put(frame_bytes)

            return True

        except Exception:
            return False

    async def _send_loop(self) -> None:
        try:
            while self.connected:
                frame_bytes = await self.audio_queue.get()

                if frame_bytes is None:
                    break

                audio_b64 = base64.b64encode(frame_bytes).decode("utf-8")
                audio_message = {"type": WS_MSG_TYPE_AUDIO, "audio": audio_b64}
                await self.websocket.send(json.dumps(audio_message))

        except asyncio.CancelledError:
            pass
        except Exception:
            self.connected = False

    async def _receive_loop(self) -> None:
        try:
            while self.connected and self.websocket:
                message = await self.websocket.recv()
                data = json.loads(message)

                msg_type = data.get("type")

                if msg_type == WS_MSG_TYPE_TEXT:
                    await self._handle_text_message(data)
                elif msg_type == WS_MSG_TYPE_VAD:
                    await self._handle_vad_message(data)

        except asyncio.CancelledError:
            pass
        except Exception as e:
            self.connected = False
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(), code="receive_error", message=str(e)
                ),
            )

    async def _handle_text_message(self, data: dict) -> None:
        try:
            text = data.get("text", "")
            start_ms = int(data.get("start_ms", 0))
            end_ms = int(data.get("end_ms", 0))
            is_final = data.get("final", False)

            duration_ms = end_ms - start_ms

            await self._handle_asr_result(
                text=text,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
            )

        except Exception:
            pass

    async def _handle_vad_message(self, data: dict) -> None:
        # VAD messages can be used for additional features
        pass

    @override
    async def finalize(self, session_id: str | None) -> None:
        # Gradium doesn't require explicit finalization per session
        pass

    @override
    def is_connected(self) -> bool:
        return self.connected

    @override
    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else GRADIUM_SAMPLE_RATE

    @override
    def input_audio_channels(self) -> int:
        return self.config.channels if self.config else 1

    @override
    def input_audio_sample_width(self) -> int:
        return self.config.bits_per_sample // 8 if self.config else 2

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeDiscard()
