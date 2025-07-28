from datetime import datetime
import json
import traceback
from typing import Any, Dict, List
from pydantic import BaseModel
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ErrorMessage, ErrorMessageVendorInfo, ModuleType
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    StatusCode,
    CmdResult,
)

import asyncio
import azure.cognitiveservices.speech as speechsdk
from dataclasses import dataclass, field


@dataclass
class AzureASRConfig(BaseModel):
    api_key: str = ""
    region: str = ""
    language: str = "en-US"
    model: str = "nova-2"
    sample_rate: int = 16000
    azure_log_path: str = "azure.log"
    params: Dict[str, Any] = field(default_factory=dict)
    black_list_params: List[str] = field(default_factory=lambda: [])

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class AzureASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.connected = False
        self.client: speechsdk.SpeechRecognizer = None
        self.stream: speechsdk.audio.PushAudioInputStream = None
        self.config: AzureASRConfig = None
        self.last_finalize_timestamp: int = 0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("AzureASRExtension on_init")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)
        await self.start_connection()

    async def _on_message(self, _, result):
        try:
            sentence = result.channel.alternatives[0].transcript

            if not sentence:
                return

            start_ms = int(
                result.start * 1000
            )  # convert seconds to milliseconds
            duration_ms = int(
                result.duration * 1000
            )  # convert seconds to milliseconds

            is_final = result.is_final
            final_from_finalize = is_final and result.from_finalize
            await self._finalize_counter_if_needed(final_from_finalize)
            self.ten_env.log_info(
                f"azure got sentence: [{sentence}], is_final: {is_final}"
            )

            transcription = UserTranscription(
                text=sentence,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
                words=[],
            )
            await self.send_asr_transcription(transcription)
        except Exception as e:
            self.ten_env.log_error(f"Error processing message: {e}")
            await self.send_asr_error(
                ErrorMessage(
                    code=1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.STT,
                ),
                None,
            )

    async def _on_recognizing(self, evt: speechsdk.SpeechRecognitionEventArgs):
        """Handle the recognizing event from Azure ASR."""
        result = json.loads(evt.result.json)
        self.ten_env.log_debug(f"azure event callback on_recognizing: {result}")
        text = result.get("Text", "")
        start_ms = (
            result.get("Offset", 0) // 10000
        )  # Convert ticks to milliseconds
        duration_ms = result.get("Duration", 0) // 10000  # Convert
        await self._on_recognized_result(
            text, final=False, start_ms=start_ms, duration_ms=duration_ms
        )

    async def _on_recognized(self, evt: speechsdk.SpeechRecognitionEventArgs):
        """Handle the recognized event from Azure ASR."""
        result = json.loads(evt.result.json)
        self.ten_env.log_debug(f"azure event callback on_recognizing: {result}")
        text = result.get("DisplayText", "")
        start_ms = (
            result.get("Offset", 0) // 10000
        )  # Convert ticks to milliseconds
        duration_ms = result.get("Duration", 0) // 10000  # Convert
        await self._on_recognized_result(
            text, final=True, start_ms=start_ms, duration_ms=duration_ms
        )

    async def _on_recognized_result(
        self, text: str, final: bool, start_ms: int = 0, duration_ms: int = 0
    ):
        """Handle the recognized result from Azure ASR."""
        try:
            transcription = UserTranscription(
                text=text,
                final=final,
                start_ms=start_ms,  # Placeholder, actual start time should be set
                duration_ms=duration_ms,  # Placeholder, actual duration should be set
                language=self.config.language,
                words=[],
                metadata={
                    "session_id": self.session_id,
                },
            )
            await self.send_asr_transcription(transcription)
        except Exception as e:
            self.ten_env.log_error(f"Error processing recognized result: {e}")
            await self.send_asr_error(
                ErrorMessage(
                    code=1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.STT,
                ),
                None,
            )

    async def _on_session_started(self, evt):
        """Handle the session started event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_session_started: {evt}"
        )
        self.connected = True

    async def _on_session_stopped(self, evt):
        """Handle the session stopped event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_session_stopped: {evt}"
        )
        self.connected = False
        if not self.stopped:
            self.ten_env.log_warn(
                "azure session stopped unexpectedly. Reconnecting..."
            )
            asyncio.create_task(self._handle_reconnect())

    async def _on_canceled(self, evt):
        """Handle the canceled event from Azure ASR."""
        self.ten_env.log_error(f"azure event callback on_canceled: {evt}")

        details = speechsdk.CancellationDetails(evt.result)
        self.ten_env.log_error(
            f"[azure] CANCELED: reason={details.reason}, error_code={details.code}, details={details.error_details}"
        )

        await self.send_asr_error(
            ErrorMessage(
                code=-1,
                message="received on_canceled event from Azure ASR",
                turn_id=0,
                module=ModuleType.STT,
            ),
            ErrorMessageVendorInfo(
                vendor="azure",
                code=details.code,
                message=details.error_details,
            ),
        )

    async def start_connection(self) -> None:
        self.ten_env.log_info("start and listen azure")
        try:

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = AzureASRConfig.model_validate_json(config_json)
                self.ten_env.log_info(f"config: {self.config}")

                if not self.config.api_key or not self.config.region:
                    self.ten_env.log_error(
                        "get property api_key or region failed"
                    )
                    return

            await self.stop_connection()

            stream_format = speechsdk.audio.AudioStreamFormat(
                channels=self.input_audio_channels(),
                samples_per_second=self.input_audio_sample_rate(),
                bits_per_sample=self.input_audio_sample_width() * 8,
                wave_stream_format=speechsdk.AudioStreamWaveFormat.PCM,
            )

            self.stream = speechsdk.audio.PushAudioInputStream(
                stream_format=stream_format
            )
            audio_config = speechsdk.audio.AudioConfig(stream=self.stream)

            speech_config = speechsdk.SpeechConfig(
                subscription=self.config.api_key,
                region=self.config.region,
            )

            if self.config.azure_log_path:
                speech_config.set_property(
                    speechsdk.PropertyId.Speech_LogFilename,
                    self.config.azure_log_path,
                )

            # Update options with params
            if self.config.params:
                for key, value in self.config.params.items():
                    # Check if it's a valid option and not in black list
                    if not self.config.is_black_list_params(key):
                        self.ten_env.log_debug(
                            f"set azure param: {key} = {value}"
                        )
                        speech_config.set_property(key, value)

            # Set the Speech_SegmentationSilenceTimeoutMs parameter to 3500ms
            # speech_config.set_property(speechsdk.PropertyId.Speech_SegmentationSilenceTimeoutMs, "3500")

            self.client = speechsdk.SpeechRecognizer(
                speech_config=speech_config,
                audio_config=audio_config,
            )

            loop = asyncio.get_running_loop()

            self.client.recognizing.connect(
                lambda evt: loop.call_soon_threadsafe(
                    asyncio.create_task, self._on_recognizing(evt)
                )
            )
            self.client.recognized.connect(
                lambda evt: loop.call_soon_threadsafe(
                    asyncio.create_task, self._on_recognized(evt)
                )
            )
            self.client.session_started.connect(
                lambda evt: loop.call_soon_threadsafe(
                    asyncio.create_task, self._on_session_started(evt)
                )
            )
            self.client.session_stopped.connect(
                lambda evt: loop.call_soon_threadsafe(
                    asyncio.create_task, self._on_session_stopped(evt)
                )
            )
            self.client.canceled.connect(
                lambda evt: loop.call_soon_threadsafe(
                    asyncio.create_task, self._on_canceled(evt)
                )
            )

            result_future = self.client.start_continuous_recognition_async()
            await loop.run_in_executor(None, result_future.get)
            self.ten_env.log_info("start_connection completed")
        except Exception as e:
            self.ten_env.log_error(
                f"Error starting azure connection: {traceback.format_exc()}"
            )
            await self.send_asr_error(
                ErrorMessage(
                    code=1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.STT,
                ),
                None,
            )
            await self._handle_reconnect()

    async def stop_connection(self) -> None:
        try:
            if self.client:
                loop = asyncio.get_running_loop()
                result_future = self.client.stop_continuous_recognition_async()
                await loop.run_in_executor(None, result_future.get)
                self.client = None
                self.connected = False
                self.ten_env.log_info("azure connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping azure connection: {e}")

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> None:
        frame_buf = frame.get_buf()
        self.stream.write(bytes(frame_buf))

    def is_connected(self) -> bool:
        return self.connected and self.client is not None

    async def finalize(self, session_id: str | None) -> None:
        # self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        # self.ten_env.log_debug(
        #     f"azure drain start at {self.last_finalize_timestamp} session_id: {session_id}"
        # )

        # TODO
        # await self.client.finalize()

        raise NotImplementedError("Azure ASR has no finalize method yet.")

    async def _finalize_counter_if_needed(self, is_final: bool) -> None:
        if is_final and self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT azure drain end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end(latency)

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate
