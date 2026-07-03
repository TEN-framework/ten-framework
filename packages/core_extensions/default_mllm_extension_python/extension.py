from typing_extensions import override

from dataclasses import dataclass

from pydantic import BaseModel
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
)
from ten_ai_base.mllm import AsyncMLLMBaseExtension
from ten_ai_base.struct import (
    MLLMClientFunctionCallOutput,
    MLLMClientMessageItem,
)
from ten_ai_base.types import (
    LLMToolMetadata,
    MLLMBufferConfig,
    MLLMBufferConfigModeDiscard,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)


@dataclass
class DefaultMLLMConfig(BaseModel):
    pass


class DefaultMLLMExtension(AsyncMLLMBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: DefaultMLLMConfig | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        ten_env.log_info("DefaultMLLMExtension on_init")

        config_json, _ = await self.ten_env.get_property_to_json("")

        try:
            self.config = DefaultMLLMConfig.model_validate_json(config_json)
        except Exception as e:
            await self._handle_error(e)

    @override
    def vendor(self) -> str:
        """Get the name of the MLLM vendor."""
        raise NotImplementedError("Vendor method not implemented")

    @override
    async def start_connection(self) -> None:
        """Start the connection to the MLLM service."""
        raise NotImplementedError("start_connection method not implemented")

    @override
    def is_connected(self) -> bool:
        """Check if the MLLM service is connected."""
        raise NotImplementedError("is_connected method not implemented")

    @override
    async def stop_connection(self) -> None:
        """Stop the connection to the MLLM service."""
        raise NotImplementedError("stop_connection method not implemented")

    @override
    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        """
        Send a message item to the MLLM service.
        This method is used to send user or assistant messages.
        """
        raise NotImplementedError(
            "send_client_message_item method not implemented"
        )

    @override
    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        """
        Send a create response to the MLLM service.
        This method is used to trigger MLLM to generate a response.
        """
        raise NotImplementedError(
            "send_client_create_response method not implemented"
        )

    @override
    async def send_client_register_tool(self, tools: LLMToolMetadata) -> None:
        """
        Register tools with the MLLM service.
        This method is used to register tools that can be called by the LLM.
        """
        raise NotImplementedError(
            "send_client_register_tool method not implemented"
        )

    @override
    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        """
        Send a function call output to the MLLM service.
        This method is used to send the result of a function call made by the LLM.
        """
        raise NotImplementedError(
            "send_client_function_call_output method not implemented"
        )

    @override
    def input_audio_sample_rate(self) -> int:
        """
        Get the input audio sample rate in Hz.
        """
        return 24000

    @override
    def synthesize_audio_sample_rate(self) -> int:
        """
        Get the input audio sample rate in Hz.
        """
        return 24000

    def buffer_strategy(self) -> MLLMBufferConfig:
        """
        Get the buffer strategy for audio frames when not connected
        """
        return MLLMBufferConfigModeDiscard()

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """
        Send an audio frame to the MLLM service, returning True if successful.
        Note: The first successful send_audio call will be timestamped for TTFW calculation.
        """
        raise NotImplementedError("send_audio method not implemented")

    async def _handle_error(self, error: Exception):
        self.ten_env.log_error(f"Default error: {error}")
        await self.send_mllm_error(
            ModuleError(
                module=ModuleType.MLLM,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=str(error),
            ),
        )
