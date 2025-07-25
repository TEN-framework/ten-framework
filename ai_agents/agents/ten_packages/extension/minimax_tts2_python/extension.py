#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback
import uuid

from ten_ai_base.helper import PCMWriter, generate_file_name
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    ModuleVendorException,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .config import MinimaxTTS2Config
from .minimax_tts import MinimaxTTS2, MinimaxTTSTaskFailedException
from ten_runtime import (
    AsyncTenEnv,
)


class MinimaxTTS2Extension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: MinimaxTTS2Config | None = None
        self.client: MinimaxTTS2 | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.recorder: PCMWriter | None = None
        self.sent_ts: datetime | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.ten_env.log_info(f"Raw config JSON: {config_json}")

                # Check if config is empty or missing required fields
                if not config_json or config_json.strip() == "{}":
                    error_msg = "Configuration is empty. Required parameters: api_key, group_id are missing."
                    self.ten_env.log_error(error_msg)

                    # Generate a temporary request ID for error reporting during initialization
                    temp_request_id = str(uuid.uuid4())
                    await self.send_tts_error(
                        temp_request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=None,
                        ),
                    )
                    return

                try:
                    self.config = MinimaxTTS2Config.model_validate_json(config_json)
                except Exception as validation_error:
                    error_msg = f"Configuration validation failed: {str(validation_error)}"
                    self.ten_env.log_error(error_msg)

                    temp_request_id = str(uuid.uuid4())
                    await self.send_tts_error(
                        temp_request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=None,
                        ),
                    )
                    return

                self.ten_env.log_info(f"Parsed config: {self.config.to_str()}")

                # Check if API key is still a placeholder
                if self.config.api_key.startswith("${env:"):
                    error_msg = f"Environment variable not resolved: {self.config.api_key}. Please set the MINIMAX_TTS_API_KEY environment variable."
                    self.ten_env.log_error(error_msg)

                    temp_request_id = str(uuid.uuid4())
                    await self.send_tts_error(
                        temp_request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=None,
                        ),
                    )
                    return

                if not self.config.api_key:
                    error_msg = "Required parameter 'api_key' is missing or empty."
                    self.ten_env.log_error(error_msg)

                    temp_request_id = str(uuid.uuid4())
                    await self.send_tts_error(
                        temp_request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=None,
                        ),
                    )
                    return

                if not self.config.group_id:
                    error_msg = "Required parameter 'group_id' is missing or empty."
                    self.ten_env.log_error(error_msg)

                    temp_request_id = str(uuid.uuid4())
                    await self.send_tts_error(
                        temp_request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=None,
                        ),
                    )
                    return

                # extract audio_params and additions from config
                self.config.update_params()

            self.recorder = PCMWriter(
                os.path.join(
                    self.config.dump_path, generate_file_name("agent_dump")
                )
            )
            self.client = MinimaxTTS2(self.config, ten_env, self.vendor())
            # Preheat websocket connection
            await self.client.start()
            ten_env.log_info("MinimaxTTS2 client initialized and preheated successfully")
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")

            # Send FATAL ERROR for unexpected exceptions during initialization
            temp_request_id = str(uuid.uuid4())
            await self.send_tts_error(
                temp_request_id,
                ModuleError(
                    message=f"Unexpected error during initialization: {str(e)}",
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=None,
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # Clean up client if exists
        if self.client:
            # Stop the websocket connection
            await self.client.stop()
            self.client = None

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "minimax"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            if t.text.strip() == "":
                self.ten_env.log_info("Received empty text for TTS request")
                return

            self.ten_env.log_info(f"TTS request with ID: {t.request_id}, text: {t.text}")
            self.current_request_id = t.request_id

            if t.metadata is not None:
                self.session_id = t.metadata.session_id
                self.current_turn_id = t.metadata.turn_id

            # Check if client is initialized
            if self.client is None:
                self.ten_env.log_error("TTS client is not initialized")
                raise RuntimeError("TTS client is not initialized")

            # Record TTFB timing
            if self.sent_ts is None:
                self.sent_ts = datetime.now()

            # Get audio stream from Minimax TTS
            self.ten_env.log_info(f"Calling client.get() with text: {t.text}")
            data = self.client.get(t.text)
            self.ten_env.log_info(f"Got data generator: {data}")
            first_chunk = True

            self.ten_env.log_info("Starting async for loop to process audio chunks")
            chunk_count = 0
            async for audio_chunk in data:
                chunk_count += 1
                self.ten_env.log_info(f"Received audio chunk #{chunk_count}, size: {len(audio_chunk) if audio_chunk else 0} bytes")

                # Process audio chunk similar to copy.py logic
                if audio_chunk and len(audio_chunk) > 0:
                    # Send TTFB metrics for first chunk
                    if first_chunk and self.sent_ts is not None:
                        elapsed_time = datetime.now() - self.sent_ts
                        await self.send_tts_ttfb_metrics(
                            self.current_request_id,
                            elapsed_time,
                            self.current_turn_id,
                        )
                        self.sent_ts = None
                        self.ten_env.log_info(
                            f"Sent TTFB metrics for request ID: {self.current_request_id}, elapsed time: {elapsed_time}"
                        )
                        first_chunk = False

                    # Write to dump file if needed
                    if self.config and self.config.dump and self.recorder:
                        #self.ten_env.log_info(f"writing to dump file")
                        asyncio.create_task(self.recorder.write(audio_chunk))

                    # Send audio data - this corresponds to on_audio_bytes in copy.py
                    await self.send_tts_audio_data(audio_chunk)

        except MinimaxTTSTaskFailedException as e:
            self.ten_env.log_error(
                f"MinimaxTTSTaskFailedException in request_tts: {e.error_msg} (code: {e.error_code}). text: {t.text}"
            )
            if e.error_code == 2054:
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=e.error_msg,
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.error_code),
                            message=e.error_msg,
                        ),
                    ),
                )
            else:
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=e.error_msg,
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.error_code),
                            message=e.error_msg,
                        ),
                    ),
                )
        except ModuleVendorException as e:
            self.ten_env.log_error(
                f"ModuleVendorException in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=e.error,
                ),
            )
        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=None,
                ),
            )