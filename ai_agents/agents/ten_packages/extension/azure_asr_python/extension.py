from datetime import datetime
import json
import os

from typing_extensions import override
from .const import (
    FINALIZE_MODE_DISCONNECT,
    FINALIZE_MODE_MUTE_PKG,
    DUMP_FILE_NAME,
    MODULE_NAME_ASR,
)
from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.message import ModuleError, ModuleErrorVendorInfo, ModuleErrorCode
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)

import asyncio
import azure.cognitiveservices.speech as speechsdk
from .config import AzureASRConfig
from ten_ai_base.dumper import Dumper
from ten_ai_base.timeline import AudioTimeline
from .reconnect_manager import ReconnectManager


class AzureASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.client: speechsdk.SpeechRecognizer | None = None
        self.connection: speechsdk.Connection | None = None
        self.stream: speechsdk.audio.PushAudioInputStream | None = None
        self.config: AzureASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.timeline: AudioTimeline = AudioTimeline()
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0

        # Reconnection manager with retry limits and backoff strategy
        self.reconnect_manager: ReconnectManager | None = None

    @override
    def vendor(self) -> str:
        return "microsoft"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = AzureASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}"
            )

            if self.config.dump:
                dump_file_path = os.path.join(self.config.dump_path, DUMP_FILE_NAME)
                self.audio_dumper = Dumper(dump_file_path)
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = AzureASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        assert self.config is not None
        self.ten_env.log_info("start_connection")

        try:
            speech_config = speechsdk.SpeechConfig(
                subscription=self.config.key, region=self.config.region
            )
        except Exception as e:
            self.ten_env.log_error(
                f"KEYPOINT start_connection failed: invalid vendor config: {e}"
            )
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

            return

        stream_format = speechsdk.audio.AudioStreamFormat(
            channels=self.input_audio_channels(),
            samples_per_second=self.input_audio_sample_rate(),
            bits_per_sample=self.input_audio_sample_width() * 8,
            wave_stream_format=speechsdk.audio.AudioStreamWaveFormat.PCM,
        )

        self.stream = speechsdk.audio.PushAudioInputStream(stream_format=stream_format)
        audio_config = speechsdk.audio.AudioConfig(stream=self.stream)

        # Set the silence timeout to 100ms by default.
        speech_config.set_property(
            speechsdk.PropertyId.Speech_SegmentationSilenceTimeoutMs, "100"
        )

        # Dump the Azure SDK log to the dump path if dump is enabled.
        if self.config.dump and self.config.dump_path:
            azure_log_file_path = os.path.join(self.config.dump_path, "azure_sdk.log")
            speech_config.set_property(
                speechsdk.PropertyId.Speech_LogFilename, azure_log_file_path
            )

        if self.config.advanced_params_json:
            try:
                params: dict[str, str] = json.loads(self.config.advanced_params_json)
                for key, value in params.items():
                    self.ten_env.log_debug(f"set azure param: {key} = {value}")
                    speech_config.set_property_by_name(key, value)
            except Exception as e:
                self.ten_env.log_error(f"set azure param failed: {e}")

        if len(self.config.language_list) > 1:
            self.client = speechsdk.SpeechRecognizer(
                speech_config=speech_config,
                audio_config=audio_config,
                auto_detect_source_language_config=speechsdk.AutoDetectSourceLanguageConfig(
                    languages=self.config.language_list
                ),
            )
        else:
            self.client = speechsdk.SpeechRecognizer(
                speech_config=speech_config,
                audio_config=audio_config,
                language=self.config.primary_language(),
            )

        if len(self.config.phrase_list) > 0:
            phrase_list_grammar = speechsdk.PhraseListGrammar.from_recognizer(
                self.client
            )
            for phrase in self.config.phrase_list:
                phrase_list_grammar.addPhrase(phrase)

        if self.audio_dumper:
            await self.audio_dumper.start()

        await self._register_azure_event_handlers()
        self.client.start_continuous_recognition()
        self.ten_env.log_info("start_connection completed")

    @override
    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        _ = self.ten_env.log_debug(
            f"KEYPOINT finalize start at {self.last_finalize_timestamp}]"
        )
        if self.config.finalize_mode == FINALIZE_MODE_DISCONNECT:
            await self._handle_finalize_disconnect()
        elif self.config.finalize_mode == FINALIZE_MODE_MUTE_PKG:
            await self._handle_finalize_mute_pkg()
        else:
            _ = self.ten_env.log_error(
                f"Unknown finalize mode: {self.config.finalize_mode}"
            )

    async def _register_azure_event_handlers(self):
        loop = asyncio.get_running_loop()
        assert self.client is not None
        self.client.recognizing.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_recognizing(evt)
            )
        )
        self.client.recognized.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_recognized(evt)
            )
        )
        self.client.session_started.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_session_started(evt)
            )
        )
        self.client.session_stopped.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_session_stopped(evt)
            )
        )
        self.client.canceled.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_canceled(evt)
            )
        )
        self.client.speech_start_detected.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task,
                self._azure_event_handler_on_speech_start_detected(evt),
            )
        )
        self.client.speech_end_detected.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task,
                self._azure_event_handler_on_speech_end_detected(evt),
            )
        )

        self.connection = speechsdk.Connection.from_recognizer(self.client)
        self.connection.connected.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_connected(evt)
            )
        )
        self.connection.disconnected.connect(
            lambda evt: loop.call_soon_threadsafe(
                asyncio.create_task, self._azure_event_handler_on_disconnected(evt)
            )
        )

    async def _handle_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int = 0,
        duration_ms: int = 0,
        language: str = "",
    ):
        """Handle the ASR result from Azure ASR."""
        assert self.config is not None

        if final:
            await self._finalize_end()

        asr_result = ASRResult(
            text=text,
            final=final,
            start_ms=start_ms,
            duration_ms=duration_ms,
            language=language,
        )

        await self.send_asr_result(asr_result)

    async def _azure_event_handler_on_recognizing(
        self, evt: speechsdk.SpeechRecognitionEventArgs
    ):
        """Handle the recognizing event from Azure ASR."""
        assert self.config is not None

        text = evt.result.text
        start_ms = evt.result.offset // 10000
        duration_ms = evt.result.duration // 10000
        actual_start_ms = int(
            self.timeline.get_audio_duration_before_time(start_ms)
            + self.sent_user_audio_duration_ms_before_last_reset
        )
        language = self.config.primary_language()
        if len(self.config.language_list) > 1:
            try:
                result_json = json.loads(evt.result.json)
                language_in_result: str = result_json["PrimaryLanguage"]["Language"]
                if language_in_result != "":
                    language = language_in_result
            except Exception as e:
                self.ten_env.log_error(f"get language from result json failed: {e}")

        if evt.result.no_match_details:
            self.ten_env.log_error(
                f"azure event callback on_recognizing: no match details: {evt.result.no_match_details}"
            )

        self.ten_env.log_debug(
            f"azure event callback on_recognizing: {text}, language: {language}, full_json: {evt.result.json}"
        )

        await self._handle_asr_result(
            text,
            final=False,
            start_ms=actual_start_ms,
            duration_ms=duration_ms,
            language=language,
        )

    async def _azure_event_handler_on_recognized(
        self, evt: speechsdk.SpeechRecognitionEventArgs
    ):
        """Handle the recognized event from Azure ASR."""
        assert self.config is not None

        text = evt.result.text
        start_ms = evt.result.offset // 10000
        duration_ms = evt.result.duration // 10000
        actual_start_ms = int(
            self.timeline.get_audio_duration_before_time(start_ms)
            + self.sent_user_audio_duration_ms_before_last_reset
        )
        language = self.config.primary_language()
        if len(self.config.language_list) > 1:
            try:
                result_json = json.loads(evt.result.json)
                language_in_result: str = result_json["PrimaryLanguage"]["Language"]
                if language_in_result != "":
                    language = language_in_result
            except Exception as e:
                self.ten_env.log_error(f"get language from result json failed: {e}")

        if evt.result.no_match_details:
            self.ten_env.log_error(
                f"azure event callback on_recognized: no match details: {evt.result.no_match_details}"
            )

        self.ten_env.log_debug(
            f"azure event callback on_recognized: {text}, language: {language}, full_json: {evt.result.json}"
        )
        await self._handle_asr_result(
            text,
            final=True,
            start_ms=actual_start_ms,
            duration_ms=duration_ms,
            language=language,
        )

    async def _azure_event_handler_on_session_started(
        self, evt: speechsdk.SessionEventArgs
    ):
        """Handle the session started event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_session_started, session_id: {evt.session_id}"
        )
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.timeline.get_total_user_audio_duration()
        )
        self.timeline.reset()
        self.connected = True

    async def _azure_event_handler_on_session_stopped(
        self, evt: speechsdk.SessionEventArgs
    ):
        """Handle the session stopped event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_session_stopped, session_id: {evt.session_id}"
        )
        self.connected = False

        if not self.stopped:
            self.ten_env.log_warn("azure session stopped unexpectedly. Reconnecting...")
            await self._handle_reconnect()

    async def _azure_event_handler_on_canceled(
        self, evt: speechsdk.SpeechRecognitionCanceledEventArgs
    ):
        """Handle the canceled event from Azure ASR."""

        cancellation_details = evt.cancellation_details
        self.ten_env.log_error(
            f"KEYPOINT vendor_error, code: {cancellation_details.code}, reason: {cancellation_details.reason}, error_details: {cancellation_details.error_details}"
        )
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=cancellation_details.error_details,
            ),
            ModuleErrorVendorInfo(
                vendor="microsoft",
                code=str(cancellation_details.code),
                message=cancellation_details.error_details,
            ),
        )

    async def _azure_event_handler_on_speech_start_detected(
        self, evt: speechsdk.RecognitionEventArgs
    ):
        """Handle the speech start detected event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_speech_start_detected, session_id: {evt.session_id}"
        )

    async def _azure_event_handler_on_speech_end_detected(
        self, evt: speechsdk.RecognitionEventArgs
    ):
        """Handle the speech end detected event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_speech_end_detected, session_id: {evt.session_id}"
        )

    async def _azure_event_handler_on_connected(
        self, evt: speechsdk.ConnectionEventArgs
    ):
        """Handle the connected event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_connected, session_id: {evt.session_id}"
        )

        # Notify reconnect manager that connection is successful
        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()

    async def _azure_event_handler_on_disconnected(
        self, evt: speechsdk.ConnectionEventArgs
    ):
        """Handle the disconnected event from Azure ASR."""
        self.ten_env.log_debug(
            f"azure event callback on_disconnected, session_id: {evt.session_id}"
        )

    async def _handle_finalize_disconnect(self):
        assert self.config is not None

        if self.client is None:
            _ = self.ten_env.log_debug("finalize disconnect: client is not connected")
            return

        self.client.stop_continuous_recognition()
        _ = self.ten_env.log_debug("finalize disconnect completed")

    async def _handle_finalize_mute_pkg(self):
        assert self.config is not None

        if self.stream is None:
            _ = self.ten_env.log_debug("finalize mute pkg: stream is not initialized")
            return

        empty_audio_bytes_len = int(
            self.config.mute_pkg_duration_ms * self.config.sample_rate / 1000 * 2
        )
        frame = bytearray(empty_audio_bytes_len)
        self.stream.write(bytes(frame))
        self.timeline.add_silence_audio(self.config.mute_pkg_duration_ms)
        self.ten_env.log_debug("finalize mute pkg completed")

    async def _handle_reconnect(self):
        """
        Handle a single reconnection attempt using the ReconnectManager.
        Connection success is determined by the _azure_event_handler_on_connected callback.

        This method should be called repeatedly (e.g., after session_stopped events)
        until either connection succeeds or max attempts are reached.
        """
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        # Check if we can still retry
        if not self.reconnect_manager.can_retry():
            self.ten_env.log_warn("No more reconnection attempts allowed")
            return

        # Attempt a single reconnection
        success = await self.reconnect_manager.handle_reconnect(
            connection_func=self.start_connection, error_handler=self.send_asr_error
        )

        if success:
            self.ten_env.log_debug("Reconnection attempt initiated successfully")
        else:
            info = self.reconnect_manager.get_attempts_info()
            self.ten_env.log_debug(f"Reconnection attempt failed. Status: {info}")

    async def _finalize_end(self) -> None:
        if self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT finalize end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def stop_connection(self) -> None:
        if self.client:
            self.client.stop_continuous_recognition()
            self.client = None
            self.connected = False
            self.ten_env.log_info("azure connection stopped")
            if self.audio_dumper:
                await self.audio_dumper.stop()

    @override
    def is_connected(self) -> bool:
        return self.connected and self.client is not None

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None

        return self.config.sample_rate

    @override
    async def send_audio(self, frame: AudioFrame, session_id: str | None) -> bool:
        assert self.config is not None
        assert self.stream is not None

        buf = frame.lock_buf()
        if self.audio_dumper:
            await self.audio_dumper.push_bytes(bytes(buf))
        self.timeline.add_user_audio(
            int(len(buf) / (self.config.sample_rate / 1000 * 2))
        )
        self.stream.write(bytes(buf))
        frame.unlock_buf(buf)

        return True
