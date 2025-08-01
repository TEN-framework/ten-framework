#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback
from typing import Tuple


from ten_ai_base.helper import PCMWriter, generate_file_name
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    ModuleVendorException,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .config import BytedanceTTSDuplexConfig

from .bytedance_tts import (
    BytedanceV3Client,
    EVENT_SessionFinished,
    EVENT_TTSResponse,
)
from ten_runtime import (
    AsyncTenEnv,
)


class BytedanceTTSDuplexExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BytedanceTTSDuplexConfig = None
        self.client: BytedanceV3Client = None
        self.current_request_id: str = None
        self.current_turn_id: int = -1
        self.stop_event: asyncio.Event = None
        self.msg_polling_task: asyncio.Task = None
        self.recorder: PCMWriter = None
        self.request_start_ts: datetime | None = None
        self.request_ttfb: int | None = None
        self.request_total_audio_duration: int | None = None
        self.response_msgs = asyncio.Queue[Tuple[int, bytes]]()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = BytedanceTTSDuplexConfig.model_validate_json(
                    config_json
                )
                self.ten_env.log_info(
                    f"KEYPOINT config: {self.config.to_str()}"
                )

                if not self.config.appid:
                    self.ten_env.log_error("get property appid")
                    raise ValueError("appid is required")

                if not self.config.token:
                    self.ten_env.log_error("get property token")
                    raise ValueError("token is required")

                # extract audio_params and additions from config
                self.config.update_params()

            self.recorder = PCMWriter(
                os.path.join(
                    self.config.dump_path, generate_file_name("agent_dump")
                )
                # based on request id
            )

            await self._start_connection()
            self.msg_polling_task = asyncio.create_task(self._loop())
        except Exception as e:
            ten_env.log_error(f"on_start failed: {traceback.format_exc()}")
            await self.send_tts_error(
                self.current_request_id or "",
                ModuleError(
                    message=str(e),
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={},
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await self._stop_connection()
        if self.msg_polling_task:
            self.msg_polling_task.cancel()

        # Flush the recorder to ensure all buffered data is written to the dump file.
        if self.recorder:
            await self.recorder.flush()
            self.recorder = None
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def _loop(self) -> None:
        while True:
            try:
                event, audio_data = await self.client.response_msgs.get()

                if event == EVENT_TTSResponse:
                    if audio_data is not None:
                        if self.config.dump:
                            asyncio.create_task(self.recorder.write(audio_data))
                        if (
                            self.request_start_ts is not None
                            and self.request_ttfb is None
                        ):
                            await self.send_tts_audio_start(
                                self.current_request_id
                            )
                            elapsed_time = int(
                                (
                                    datetime.now() - self.request_start_ts
                                ).total_seconds()
                                * 1000
                            )
                            await self.send_tts_ttfb_metrics(
                                self.current_request_id,
                                elapsed_time,
                                self.current_turn_id,
                            )
                            self.request_ttfb = elapsed_time
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTFB metrics for request ID: {self.current_request_id}, elapsed time: {elapsed_time}ms"
                            )
                        self.request_total_audio_duration += (
                            self.calculate_audio_duration(
                                len(audio_data),
                                self.synthesize_audio_sample_rate(),
                                self.synthesize_audio_channels(),
                                self.synthesize_audio_sample_width(),
                            )
                        )
                        await self.send_tts_audio_data(audio_data)
                    else:
                        self.ten_env.log_error(
                            "Received empty payload for TTS response"
                        )
                elif event == EVENT_SessionFinished:
                    self.ten_env.log_info(
                        f"KEYPOINT Session finished for request ID: {self.current_request_id}"
                    )
                    if self.request_start_ts is not None:
                        request_event_interval = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
                        )
                        await self.send_tts_audio_end(
                            self.current_request_id,
                            request_event_interval,
                            self.request_total_audio_duration,
                            self.current_turn_id,
                        )

                        self.ten_env.log_info(
                            f"KEYPOINT request time stamped for request ID: {self.current_request_id}, request_event_interval: {request_event_interval}ms, total_audio_duration: {self.request_total_audio_duration}ms"
                        )
                    if self.stop_event:
                        self.stop_event.set()
                        self.stop_event = None

            except Exception:
                self.ten_env.log_error(
                    f"Error in _loop: {traceback.format_exc()}"
                )

    async def _start_connection(self) -> None:
        """
        Prepare the connection to the TTS service.
        This method is called before sending any TTS requests.
        """
        if self.client is None:
            self.client = BytedanceV3Client(
                self.config, self.ten_env, self.vendor(), self.response_msgs
            )
            self.ten_env.log_info(
                f"KEYPOINT Connecting to service for request ID: {self.current_request_id}"
            )
            await self.client.connect()
            await self.client.start_connection()
            await self.client.start_session()

    async def _stop_connection(self) -> None:
        try:
            if self.client:
                await self.client.finish_session()
                await self.client.finish_connection()
                await self.client.close()
        except Exception:
            self.ten_env.log_warn(
                f"Error during cleanup: {traceback.format_exc()}"
            )
        if self.stop_event:
            self.stop_event.set()
            self.stop_event = None
        self.client = None

    async def _reconnect(self) -> None:
        """
        Reconnect to the TTS service.
        This method is called when the connection is lost or needs to be re-established.
        """
        await self._stop_connection()
        await self._start_connection()

    def vendor(self) -> str:
        return "bytedance"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
            )
            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )
                self.current_request_id = t.request_id
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)
                self.request_start_ts = datetime.now()
                self.request_ttfb = None
                self.request_total_audio_duration = 0

            if t.text.strip() != "":
                await self.client.send_text(t.text)
            if t.text_input_end:
                self.ten_env.log_info(
                    f"KEYPOINT finish session for request ID: {t.request_id}"
                )
                await self.client.finish_session()

                self.stop_event = asyncio.Event()
                await self.stop_event.wait()

                # close connection after session is finished
                await self.client.finish_connection()
                await self.client.close()
                self.client = None

                # restart connection to prepare for the next request
                await self._start_connection()
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
            await self._reconnect()
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
                    vendor_info={},
                ),
            )
            await self._reconnect()

    def calculate_audio_duration(
        self,
        bytes_length: int,
        sample_rate: int,
        channels: int = 1,
        sample_width: int = 2,
    ) -> int:
        """
        Calculate audio duration in milliseconds.

        Parameters:
        - bytes_length: Length of the audio data in bytes
        - sample_rate: Sample rate in Hz (e.g., 16000)
        - channels: Number of audio channels (default: 1 for mono)
        - sample_width: Number of bytes per sample (default: 2 for 16-bit PCM)

        Returns:
        - Duration in milliseconds (rounded down to nearest int)
        """
        bytes_per_second = sample_rate * channels * sample_width
        duration_seconds = bytes_length / bytes_per_second
        return int(duration_seconds * 1000)
