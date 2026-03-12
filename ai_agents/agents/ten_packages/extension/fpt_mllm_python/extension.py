import asyncio
import traceback

from pydantic import BaseModel

from ten_ai_base.mllm import AsyncMLLMBaseExtension
from ten_ai_base.struct import (
    MLLMClientFunctionCallOutput,
    MLLMClientMessageItem,
    MLLMServerInputTranscript,
    MLLMServerOutputTranscript,
    MLLMServerSessionReady,
)
from ten_ai_base.types import LLMToolMetadata
from ten_runtime import AsyncTenEnv, AudioFrame, Data

from .realtime.connection import FPTTokenManager, RealtimeApiConnection
from .realtime.struct import (
    AuthError,
    AuthSuccess,
    BinaryAudioMessage,
    BridgeStatus,
    ErrorMessage,
    TranscriptMessage,
    UnknownMessage,
)


class FPTRealtimeConfig(BaseModel):
    token_url: str = ""
    websocket_url: str = ""
    api_key: str = ""
    agent_id: str = ""
    agent_type: str = "agent"
    voice: str = "default"
    voice_speed: float = 1.0
    sample_rate: int = 16000
    vendor: str = "fpt"
    dump: bool = False
    dump_path: str = ""


class FPTRealtimeExtension(AsyncMLLMBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.config: FPTRealtimeConfig | None = None
        self.conn: RealtimeApiConnection | None = None
        self.token_manager: FPTTokenManager | None = None
        self.connected = False
        self.authenticated = False
        self.call_id = ""
        self.request_transcript = ""
        self.response_transcript = ""

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env
        properties, _ = await ten_env.get_property_to_json(None)
        self.config = FPTRealtimeConfig.model_validate_json(properties)

        if not self.config.token_url:
            raise ValueError("token_url is required")
        if not self.config.websocket_url:
            raise ValueError("websocket_url is required")
        if not self.config.api_key:
            raise ValueError("api_key is required")
        if not self.config.agent_id:
            raise ValueError("agent_id is required")

        self.token_manager = FPTTokenManager(
            ten_env=ten_env,
            token_url=self.config.token_url,
            api_key=self.config.api_key,
            agent_id=self.config.agent_id,
            agent_type=self.config.agent_type,
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        if self.token_manager is not None:
            await self.token_manager.close()

    def vendor(self) -> str:
        assert self.config is not None
        return self.config.vendor

    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    def is_connected(self) -> bool:
        return self.connected

    async def start_connection(self) -> None:
        try:
            assert self.token_manager is not None
            assert self.config is not None

            self.call_id = ""
            self.connected = False
            self.authenticated = False
            self.request_transcript = ""
            self.response_transcript = ""

            token = await self.token_manager.get_token()
            self.conn = RealtimeApiConnection(
                ten_env=self.ten_env,
                websocket_url=self.config.websocket_url,
                verbose=self.config.dump,
            )
            await self.conn.connect()
            await self.conn.send_auth(
                token=token,
                agent_id=self.config.agent_id,
                agent_type=self.config.agent_type,
                voice=self.config.voice,
                voice_speed=self.config.voice_speed,
            )

            self.ten_env.log_info("FPT realtime loop started")
            async for message in self.conn.listen():
                try:
                    match message:
                        case AuthSuccess():
                            self.authenticated = True
                            self.call_id = message.call_id
                            self.ten_env.log_info(
                                "FPT websocket authenticated: "
                                f"agent_type={message.agent_type}, "
                                f"agent_id={message.agent_id}, "
                                f"call_id={message.call_id}"
                            )
                            await self.conn.send_bridge_connect(
                                message.call_id
                            )
                        case BridgeStatus():
                            self.call_id = message.call_id or self.call_id
                            if not self.connected:
                                self.connected = True
                                await self.send_server_session_ready(
                                    MLLMServerSessionReady()
                                )
                            self.ten_env.log_info(
                                "FPT bridge status: "
                                f"state={message.state}, call_id={self.call_id}"
                            )
                        case TranscriptMessage():
                            if message.direction == "input":
                                delta = self._next_delta(
                                    self.request_transcript, message.text
                                )
                                self.request_transcript = message.text
                                await self.send_server_input_transcript(
                                    MLLMServerInputTranscript(
                                        content=message.text,
                                        delta=delta,
                                        final=message.final,
                                        metadata={},
                                    )
                                )
                                if message.final:
                                    self.request_transcript = ""
                            else:
                                delta = self._next_delta(
                                    self.response_transcript, message.text
                                )
                                self.response_transcript = message.text
                                await self.send_server_output_text(
                                    MLLMServerOutputTranscript(
                                        content=message.text,
                                        delta=delta,
                                        final=message.final,
                                        metadata={},
                                    )
                                )
                                if message.final:
                                    self.response_transcript = ""
                        case BinaryAudioMessage():
                            await self.send_server_output_audio_data(
                                message.audio
                            )
                        case AuthError():
                            raise PermissionError(
                                f"FPT websocket auth failed: {message.message}"
                            )
                        case ErrorMessage():
                            self.ten_env.log_error(
                                f"FPT realtime error: {message.error}"
                            )
                        case UnknownMessage():
                            self.ten_env.log_debug(
                                f"Unhandled FPT message: {message.raw}"
                            )
                        case _:
                            self.ten_env.log_debug(
                                f"Unknown FPT event object: {message}"
                            )
                except Exception as exc:
                    traceback.print_exc()
                    self.ten_env.log_error(
                        f"Failed to process FPT message {message}: {exc}"
                    )

            self.ten_env.log_info("FPT realtime loop finished")
        except Exception as exc:
            traceback.print_exc()
            self.ten_env.log_error(f"FPT start_connection failed: {exc}")
            if self._should_refresh_token(exc):
                await self._refresh_token()

        await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.connected = False
        self.authenticated = False
        self.call_id = ""
        if self.conn is not None:
            try:
                await self.conn.send_bridge_disconnect()
            except Exception:
                pass
            await self.conn.close()
            self.conn = None

    async def _handle_reconnect(self) -> None:
        await self.stop_connection()
        if not self.stopped:
            await asyncio.sleep(1)
            await self.start_connection()

    async def _refresh_token(self) -> None:
        if self.token_manager is None:
            return
        try:
            await self.token_manager.get_token(force_refresh=True)
        except Exception as exc:
            self.ten_env.log_error(f"FPT token refresh failed: {exc}")

    def _should_refresh_token(self, exc: Exception) -> bool:
        message = str(exc).lower()
        return "401" in message or "403" in message or "auth" in message

    def _next_delta(self, current: str, updated: str) -> str:
        if updated.startswith(current):
            return updated[len(current) :]
        return updated

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        if self.conn is None or not self.connected:
            return False
        await self.conn.send_audio_data(frame.get_buf())
        return True

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        await super().on_data(ten_env, data)

    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        self.ten_env.log_warn(
            "FPT websocket text message items are not supported by this "
            "minimal voice bridge"
        )

    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        self.ten_env.log_debug(
            "Ignoring create_response for FPT minimal voice bridge"
        )

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        self.ten_env.log_warn(
            "FPT websocket tool registration is not supported"
        )

    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        self.ten_env.log_warn(
            "FPT websocket function call output is not supported"
        )
