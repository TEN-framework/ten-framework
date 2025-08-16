#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    ModuleVendorException,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput, TTSTextResult
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .elevenlabs_tts import ElevenLabsTTS2, ElevenLabsTTS2Config
from ten_runtime import (
    AsyncTenEnv,
    Data,
)


class ElevenLabsTTS2Extension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: ElevenLabsTTS2Config = None
        self.client: ElevenLabsTTS2 = None
        self.current_request_id: str = None
        self.current_turn_id: int = -1
        self.stop_event: asyncio.Event = None
        self.recorder: PCMWriter = None
        self.request_start_ts: datetime | None = None
        self.request_ttfb: int | None = None
        self.request_total_audio_duration: int | None = None
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # store different request id pcmwriter
        self.last_completed_request_id: str | None = None
        self.completed_request_ids: set[str] = set()
        self.flush_request_ids: set[str] = set()
        self.read_audio_data_task: asyncio.Task = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = ElevenLabsTTS2Config.model_validate_json(
                    config_json
                )
                self.config.update_params()
                self.ten_env.log_info(
                    f"KEYPOINT config: {self.config.to_str()}"
                )

                if not self.config.api_key:
                    self.ten_env.log_error("get property api_key")
                    raise ValueError("api_key is required")

            # Create error callback function
            async def error_callback(request_id: str, error: ModuleError):
                # If no request_id is provided, use the current request_id
                target_request_id = (
                    request_id if request_id else self.current_request_id or ""
                )
                await self.send_tts_error(target_request_id, error)
                if error.code == ModuleErrorCode.FATAL_ERROR:
                    self.ten_env.log_error(
                        f"Fatal error occurred: {error.message}"
                    )
                    await self.client.close_connection()
                    self.on_stop(self.ten_env)

            self.client = ElevenLabsTTS2(self.config, ten_env, error_callback)
            await self.client.start_connection()
            self.read_audio_data_task = asyncio.create_task(
                self.read_audio_data()
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                "",  # No request_id available during on_init
                ModuleError(
                    message=str(e),
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={},
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # Cancel audio data reading task
        if self.read_audio_data_task and not self.read_audio_data_task.done():
            self.read_audio_data_task.cancel()
            try:
                await self.read_audio_data_task
            except asyncio.CancelledError:
                pass

        # Close WebSocket connection
        if self.client:
            try:
                await self.client.close_connection()
            except Exception as e:
                ten_env.log_error(f"Error closing WebSocket connection: {e}")

        # close all PCMWriter
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_info(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "elevenlabs"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_channels(self) -> int:
        return 1

    def synthesize_audio_sample_width(self) -> int:
        return 2

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
            )

            # check if request_id is in flush_request_ids
            if t.request_id in self.flush_request_ids:
                error_msg = f"Request ID {t.request_id} was flushed, ignoring TTS request"
                self.ten_env.log_warn(error_msg)
                await self.send_tts_error(
                    t.request_id,
                    ModuleError(
                        message=error_msg,
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info={"vendor": "elevenlabs"},
                    ),
                )
                return

            # check if request_id has already been completed
            if (
                self.completed_request_ids
                and t.request_id in self.completed_request_ids
            ):
                error_msg = (
                    f"Request ID {t.request_id} has already been completed "
                )
                self.ten_env.log_warn(error_msg)
                await self.send_tts_error(
                    t.request_id,
                    ModuleError(
                        message=error_msg,
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info={"vendor": "elevenlabs"},
                    ),
                )
                return
            if t.text_input_end == True:
                self.completed_request_ids.add(t.request_id)
                self.ten_env.log_info(
                    f"add completed request_id to: {t.request_id}"
                )
            # new request id
            if (
                self.current_request_id is None
                or t.request_id != self.current_request_id
            ):
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

                # create new PCMWriter for new request_id, and clean up old PCMWriter
                if self.config.dump:
                    # clean up old PCMWriter (except for the current new request_id)
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_info(
                                f"Cleaned up old PCMWriter for request_id: {old_rid}"
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    # create new PCMWriter
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"elevenlabs_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )

            self.client.text_input_queue.put_nowait(t)

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
                    vendor_info={"vendor": "elevenlabs"},
                ),
            )

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        name = data.get_name()
        if name == "tts_flush":
            ten_env.log_info(f"Received tts_flush data: {name}")

            # get flush_id and record to flush_request_ids
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                self.flush_request_ids.add(flush_id)
                ten_env.log_info(
                    f"Added request_id {flush_id} to flush_request_ids set"
                )

            try:
                await self.client.handle_flush(flush_id)
            except Exception as e:
                ten_env.log_error(f"Error in handle_flush: {e}")
                await self.send_tts_error(
                    flush_id,
                    ModuleError(
                        message=str(e),
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info={"vendor": "elevenlabs"},
                    ),
                )
                return

            # update request_id
            self.completed_request_ids.add(flush_id)
            self.ten_env.log_info(f"add completed request_id to: {flush_id}")
            # send audio_end
            request_event_interval = 0
            if self.request_start_ts is not None:
                request_event_interval = int(
                    (datetime.now() - self.request_start_ts).total_seconds()
                    * 1000
                )
            await self.send_tts_audio_end(
                flush_id,
                request_event_interval,
                self.request_total_audio_duration,
                self.current_turn_id,
                TTSAudioEndReason.INTERRUPTED,
            )
            ten_env.log_info(
                f"Sent tts_audio_end with INTERRUPTED reason for request_id: {flush_id}"
            )
        await super().on_data(ten_env, data)

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

    async def read_audio_data(self):
        try:
            while True:
                result = await self.client.get_synthesized_audio()
                audio_data, isFinal, text = result
                # Check for special stop signal
                if text == "STOP_LOOP":
                    self.ten_env.log_info(
                        "Received STOP_LOOP signal, exiting read_audio_data"
                    )
                    break
                # check if current request_id is in flush_request_ids
                if (
                    self.current_request_id
                    and self.current_request_id in self.flush_request_ids
                ):
                    self.ten_env.log_info(
                        f"Request ID {self.current_request_id} was flushed, skipping audio data"
                    )
                else:
                    # new request_id, send TTSAudioStart event and TTFB metrics
                    if (
                        self.current_request_id
                        and self.request_start_ts is not None
                        and self.request_ttfb is None
                    ):
                        self.ten_env.log_info(
                            f"KEYPOINT Sent TTSAudioStart for request ID: {self.current_request_id}"
                        )
                        await self.send_tts_audio_start(self.current_request_id)
                        elapsed_time = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
                        )
                        if self.current_request_id:
                            await self.send_tts_ttfb_metrics(
                                self.current_request_id,
                                elapsed_time,
                                self.current_turn_id,
                            )
                        self.request_ttfb = elapsed_time
                        self.ten_env.log_info(
                            f"KEYPOINT Sent TTFB metrics for request ID: {self.current_request_id}, elapsed time: {elapsed_time}ms"
                        )
                    cur_duration = 0
                    if audio_data is not None:
                        if (
                            self.config.dump
                            and self.current_request_id
                            and self.current_request_id in self.recorder_map
                        ):
                            await self.recorder_map[
                                self.current_request_id
                            ].write(audio_data)
                        cur_duration = self.calculate_audio_duration(
                            len(audio_data),
                            self.synthesize_audio_sample_rate(),
                            self.synthesize_audio_channels(),
                            self.synthesize_audio_sample_width(),
                        )
                        if self.request_total_audio_duration is None:
                            self.request_total_audio_duration = cur_duration
                        else:
                            self.request_total_audio_duration += cur_duration
                        await self.send_tts_audio_data(audio_data)
                    if text and self.current_request_id:
                        # send text result
                        await self.send_tts_text_result(
                            TTSTextResult(
                                request_id=self.current_request_id,
                                text=text,
                                text_input_end=isFinal,
                                start_ms=0,
                                words=[],
                                duration_ms=cur_duration,
                                metadata={},
                            )
                        )

                if isFinal and self.current_request_id:
                    await self.client.start_connection()
                    self.ten_env.log_info(
                        f"Request ID {self.current_request_id} is final, send TTSAudioEnd event"
                    )
                    if self.request_start_ts is not None:
                        request_event_interval = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
                        )
                        if self.current_request_id:
                            await self.send_tts_audio_end(
                                self.current_request_id,
                                request_event_interval,
                                self.request_total_audio_duration,
                                self.current_turn_id,
                            )

                        self.ten_env.log_info(
                            f"KEYPOINT request time stamped for request ID: {self.current_request_id}, request_event_interval: {request_event_interval}ms, total_audio_duration: {self.request_total_audio_duration}ms"
                        )
                    # update request_id
                    self.completed_request_ids.add(self.current_request_id)
                    self.ten_env.log_info(
                        f"add completed request_id to: {self.current_request_id}"
                    )
        except asyncio.CancelledError:
            self.ten_env.log_info("read_audio_data task cancelled")
            raise
        except Exception:
            self.ten_env.log_error(
                f"Error in read_audio_data: {traceback.format_exc()}"
            )
