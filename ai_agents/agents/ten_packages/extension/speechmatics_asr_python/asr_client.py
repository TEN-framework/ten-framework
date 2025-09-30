#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
from typing import Awaitable, Callable, List, Optional, Coroutine
import speechmatics

from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.struct import ASRResult
from ten_ai_base.transcription import Word
from ten_runtime import AsyncTenEnv, AudioFrame
from .audio_stream import AudioStream, AudioStreamEventType
from .config import SpeechmaticsASRConfig
from .word import (
    SpeechmaticsASRWord,
    convert_words_to_sentence,
    get_sentence_duration_ms,
    get_sentence_start_ms,
    get_speaker_segments,
)
from ten_ai_base.timeline import AudioTimeline

# from .language_utils import get_speechmatics_language


async def run_asr_client(client: "SpeechmaticsASRClient"):
    assert client.client is not None
    assert client.transcription_config is not None
    assert client.audio_settings is not None

    await client.client.run(
        client.audio_stream,
        client.transcription_config,
        client.audio_settings,
    )


class SpeechmaticsASRClient:
    def __init__(
        self,
        config: SpeechmaticsASRConfig,
        ten_env: AsyncTenEnv,
        timeline: AudioTimeline,
    ):
        self.config = config
        self.ten_env = ten_env
        self.task = None
        self.audio_queue = asyncio.Queue()
        self.timeline = timeline
        self.audio_stream = AudioStream(
            self.audio_queue, self.config, self.timeline, ten_env
        )
        self.client_running_task: asyncio.Task | None = None
        self.client_needs_stopping = False
        self.sent_user_audio_duration_ms_before_last_reset = 0
        self.last_drain_timestamp: int = 0
        self.session_id = None
        self.connected = False
        # Cache the words for sentence final mode
        self.cache_words = []  # type: List[SpeechmaticsASRWord]

        self.audio_settings: speechmatics.models.AudioSettings | None = None
        self.transcription_config: (
            speechmatics.models.TranscriptionConfig | None
        ) = None
        self.client: speechmatics.client.WebsocketClient | None = None
        self.on_asr_open: Optional[
            Callable[[], Coroutine[object, object, None]]
        ] = None
        self.on_asr_result: Optional[
            Callable[[ASRResult], Coroutine[object, object, None]]
        ] = None
        self.on_asr_error: Optional[
            Callable[
                [ModuleError, Optional[ModuleErrorVendorInfo]],
                Awaitable[None],
            ]
        ] = None
        self.on_asr_close: Optional[
            Callable[[], Coroutine[object, object, None]]
        ] = None

    async def start(self) -> None:
        """Initialize and start the recognition session"""
        connection_settings = speechmatics.models.ConnectionSettings(
            url=self.config.uri,
            auth_token=self.config.key,
        )

        # sample_rate * bytes_per_sample * chunk_ms / 1000
        chunk_len = self.config.sample_rate * 2 / 1000 * self.config.chunk_ms

        self.audio_settings = speechmatics.models.AudioSettings(
            chunk_size=int(chunk_len),
            encoding=self.config.encoding,
            sample_rate=self.config.sample_rate,
        )

        additional_vocab = []
        if self.config.hotwords:
            for hw in self.config.hotwords:
                tokens = hw.split("|")
                if len(tokens) == 2 and tokens[1].isdigit():
                    additional_vocab.append({"content": tokens[0]})
                else:
                    self.ten_env.log_warn("invalid hotword format: " + hw)

        diarization_kwargs: dict = {}
        if self.config.diarization:
            diarization_kwargs["diarization"] = self.config.diarization

        if self.config.speaker_diarization_config:
            diarization_kwargs["speaker_diarization_config"] = (
                self.config.speaker_diarization_config
            )

        if self.config.channel_diarization_labels:
            diarization_kwargs["channel_diarization_labels"] = (
                self.config.channel_diarization_labels
            )

        self.transcription_config = speechmatics.models.TranscriptionConfig(
            enable_partials=self.config.enable_partials,
            language=self.config.language,
            max_delay=self.config.max_delay,
            max_delay_mode=self.config.max_delay_mode,
            additional_vocab=additional_vocab,
            operating_point=(
                self.config.operating_point
                if self.config.operating_point
                else None
            ),
            **diarization_kwargs,
        )

        # Initialize client
        self.client = speechmatics.client.WebsocketClient(connection_settings)

        # Set up callbacks
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.RecognitionStarted,
            self._handle_recognition_started,
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.EndOfTranscript,
            self._handle_end_transcript,
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.AudioEventStarted,
            self._handle_audio_event_started,
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.AudioEventEnded,
            self._handle_audio_event_ended,
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.Info, self._handle_info
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.Warning, self._handle_warning
        )
        self.client.add_event_handler(
            speechmatics.models.ServerMessageType.Error, self._handle_error
        )

        if self.config.enable_word_final_mode:
            self.client.add_event_handler(
                speechmatics.models.ServerMessageType.AddTranscript,
                self._handle_transcript_word_final_mode,
            )
            self.client.add_event_handler(
                speechmatics.models.ServerMessageType.AddPartialTranscript,
                self._handle_partial_transcript,
            )
        else:
            self.client.add_event_handler(
                speechmatics.models.ServerMessageType.AddTranscript,
                self._handle_transcript_sentence_final_mode,
            )
            # Ignore partial transcript

        self.client_needs_stopping = False
        self.client_running_task = asyncio.create_task(self._client_run())

    async def recv_audio_frame(
        self, frame: AudioFrame, session_id: str | None
    ) -> None:
        frame_buf = frame.get_buf()
        if not frame_buf:
            self.ten_env.log_warn("send_frame: empty audio_frame detected.")
            return

        self.session_id = session_id

        try:

            await self.audio_queue.put(frame_buf)
        except Exception as e:
            self.ten_env.log_error(f"Error sending audio frame: {e}")
            error = ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=str(e),
            )
            asyncio.create_task(self._emit_error(error, None))

    async def stop(self) -> None:
        self.ten_env.log_info("call stop")
        self.client_needs_stopping = True

        if self.client is not None:
            try:
                # Synchronously call stop() and then wait for the connection to close
                self.client.stop()
                self.connected = False

                # Wait for WebSocket connection to fully close
                max_wait_time = 5.0  # Maximum wait time of 5 seconds
                start_time = asyncio.get_event_loop().time()

                self.ten_env.log_debug(
                    f"Waiting for client connection to close (max {max_wait_time}s)"
                )

                while (
                    asyncio.get_event_loop().time() - start_time
                ) < max_wait_time:
                    # Check if the connection is actually closed
                    if not self.is_connected():
                        break
                    await asyncio.sleep(0.1)

                elapsed = asyncio.get_event_loop().time() - start_time
                self.ten_env.log_debug(
                    f"Client connection closed after {elapsed:.2f}s"
                )

                # Force cleanup of references
                self.client = None

            except Exception as e:
                self.ten_env.log_error(f"Error during client stop: {e}")
                # Clean up references even if there's an error
                self.client = None

        await self.audio_queue.put(AudioStreamEventType.FLUSH)
        await self.audio_queue.put(AudioStreamEventType.CLOSE)

        if self.client_running_task:
            await self.client_running_task

        self.client_running_task = None

    async def _client_run(self):
        self.ten_env.log_info("SpeechmaticsASRClient run start")

        last_connect_time = 0
        retry_interval = 0.5
        max_retry_interval = 30.0

        while not self.client_needs_stopping:
            try:
                current_time = asyncio.get_event_loop().time()
                if current_time - last_connect_time < retry_interval:
                    await asyncio.sleep(retry_interval)

                last_connect_time = current_time
                await run_asr_client(self)

                retry_interval = 0.5

            except Exception as e:
                self.ten_env.log_error(f"Error running client: {e}")
                retry_interval = min(retry_interval * 2, max_retry_interval)
                error = ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                )
                asyncio.create_task(self._emit_error(error, None))

            self.ten_env.log_info(
                "run end, client_needs_stopping:{}".format(
                    self.client_needs_stopping
                )
            )

            if self.client_needs_stopping:
                break

        self.ten_env.log_info("SpeechmaticsASRClient run end")

    async def internal_drain_mute_pkg(self):
        # we push some silence pkg to the queue
        # to trigger the final recognition result.
        await self.audio_stream.push_mute_pkg()

    async def internal_drain_disconnect(self):
        await self.audio_queue.put(AudioStreamEventType.FLUSH)
        await self.audio_queue.put(AudioStreamEventType.CLOSE)

        # wait for the client to auto reconnect

    def _handle_recognition_started(self, msg):
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.timeline.get_total_user_audio_duration()
        )

        self.ten_env.log_info(
            f"sent_user_audio_duration_ms_before_last_reset: {self.sent_user_audio_duration_ms_before_last_reset} msg: {msg}"
        )
        self.timeline.reset()
        if self.on_asr_open:
            self.connected = True
            asyncio.create_task(self.on_asr_open())

    def _handle_partial_transcript(self, msg):
        try:
            metadata = msg.get("metadata", {})
            text = metadata.get("transcript", "")
            start_ms = metadata.get("start_time", 0) * 1000
            end_ms = metadata.get("end_time", 0) * 1000
            _duration_ms = int(end_ms - start_ms)

            _actual_start_ms = int(
                self.timeline.get_audio_duration_before_time(start_ms)
                + self.sent_user_audio_duration_ms_before_last_reset
            )

            speaker = metadata.get("speaker")
            channel = metadata.get("channel")
            if channel is not None:
                channel = str(channel)

            temp_word = SpeechmaticsASRWord(
                word=text,
                start_ms=_actual_start_ms,
                duration_ms=_duration_ms,
                is_punctuation=False,
                speaker=speaker,
                channel=channel,
            )

            diarization_metadata = self._build_diarization_metadata(
                [temp_word]
            )
            asr_words = self._convert_to_asr_words([temp_word], stable=False)

            asr_result = ASRResult(
                text=text,
                final=False,
                start_ms=_actual_start_ms,
                duration_ms=_duration_ms,
                language=self.config.language,
                words=asr_words,
                metadata=diarization_metadata,
            )
            if self.on_asr_result:
                asyncio.create_task(self.on_asr_result(asr_result))
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error = ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=str(e),
            )
            asyncio.create_task(self._emit_error(error, None))

    def _handle_transcript_word_final_mode(self, msg):
        try:
            metadata = msg.get("metadata", {})
            text = metadata.get("transcript", "")
            self.ten_env.log_info(
                f"_handle_transcript_word_final_mode,metadata: {metadata}"
            )
            if text:
                start_ms = metadata.get("start_time", 0) * 1000
                end_ms = metadata.get("end_time", 0) * 1000
                _duration_ms = int(end_ms - start_ms)
                _actual_start_ms = int(
                    self.timeline.get_audio_duration_before_time(start_ms)
                    + self.sent_user_audio_duration_ms_before_last_reset
                )

                speaker = metadata.get("speaker")
                channel = metadata.get("channel")
                if channel is not None:
                    channel = str(channel)

                temp_word = SpeechmaticsASRWord(
                    word=text,
                    start_ms=_actual_start_ms,
                    duration_ms=_duration_ms,
                    is_punctuation=False,
                    speaker=speaker,
                    channel=channel,
                )

                diarization_metadata = self._build_diarization_metadata(
                    [temp_word]
                )
                asr_words = self._convert_to_asr_words(
                    [temp_word], stable=True
                )

                asr_result = ASRResult(
                    text=text,
                    final=True,
                    start_ms=_actual_start_ms,
                    duration_ms=_duration_ms,
                    language=self.config.language,
                    words=asr_words,
                    metadata=diarization_metadata,
                )

                if self.on_asr_result:
                    asyncio.create_task(self.on_asr_result(asr_result))
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error = ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=str(e),
            )
            asyncio.create_task(self._emit_error(error, None))

    def _handle_transcript_sentence_final_mode(self, msg):
        self.ten_env.log_info(
            f"_handle_transcript_sentence_final_mode, msg: {msg}"
        )

        try:
            results = msg.get("results", {})

            for result in results:
                # Get the first candidate
                alternatives = result.get("alternatives", [])
                if alternatives:
                    text = alternatives[0].get("content", "")
                    if text:
                        start_ms = result.get("start_time", 0) * 1000
                        end_ms = result.get("end_time", 0) * 1000
                        duration_ms = int(end_ms - start_ms)
                        actual_start_ms = int(
                            self.timeline.get_audio_duration_before_time(
                                start_ms
                            )
                            + self.sent_user_audio_duration_ms_before_last_reset
                        )
                        result_type = result.get("type", "")
                        is_punctuation = result_type == "punctuation"

                        speaker = alternatives[0].get("speaker")
                        channel = result.get("channel")
                        if channel is not None:
                            channel = str(channel)

                        word = SpeechmaticsASRWord(
                            word=text,
                            start_ms=actual_start_ms,
                            duration_ms=duration_ms,
                            is_punctuation=is_punctuation,
                            speaker=speaker,
                            channel=channel,
                        )
                        self.cache_words.append(word)

                if result.get("is_eos") == True:
                    sentence = convert_words_to_sentence(
                        self.cache_words, self.config
                    )
                    start_ms = get_sentence_start_ms(self.cache_words)
                    duration_ms = get_sentence_duration_ms(self.cache_words)

                    metadata = self._build_diarization_metadata(self.cache_words)
                    asr_words = self._convert_to_asr_words(
                        self.cache_words, stable=True
                    )

                    asr_result = ASRResult(
                        text=sentence,
                        final=True,
                        start_ms=start_ms,
                        duration_ms=duration_ms,
                        language=self.config.language,
                        words=asr_words,
                        metadata=metadata,
                    )

                    if self.on_asr_result:
                        asyncio.create_task(self.on_asr_result(asr_result))
                    self.cache_words = []

            # if the transcript is not empty, send it as a partial transcript
            if self.cache_words:
                sentence = convert_words_to_sentence(
                    self.cache_words, self.config
                )
                start_ms = get_sentence_start_ms(self.cache_words)
                duration_ms = get_sentence_duration_ms(self.cache_words)

                metadata = self._build_diarization_metadata(self.cache_words)
                asr_words = self._convert_to_asr_words(
                    self.cache_words, stable=False
                )

                asr_result_partial = ASRResult(
                    text=sentence,
                    final=False,
                    start_ms=start_ms,
                    duration_ms=duration_ms,
                    language=self.config.language,
                    words=asr_words,
                    metadata=metadata,
                )

                if self.on_asr_result:
                    asyncio.create_task(self.on_asr_result(asr_result_partial))
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error = ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=str(e),
            )

            asyncio.create_task(self._emit_error(error, None))

    def _handle_end_transcript(self, msg):
        self.ten_env.log_info(f"_handle_end_transcript, msg: {msg}")
        if self.on_asr_close:
            asyncio.create_task(self.on_asr_close())

    def _handle_info(self, msg):
        self.ten_env.log_info(f"_handle_info, msg: {msg}")

    def _handle_warning(self, msg):
        self.ten_env.log_warn(f"_handle_warning, msg: {msg}")

    def _handle_error(self, error):
        self.ten_env.log_error(f"_handle_error, error: {error}")
        error = ModuleError(
            module=ModuleType.ASR,
            code=ModuleErrorCode.NON_FATAL_ERROR.value,
            message=str(error),
        )

        asyncio.create_task(
            self._emit_error(
                error,
                None,
            )
        )

    def _handle_audio_event_started(self, msg):
        self.ten_env.log_info(f"_handle_audio_event_started, msg: {msg}")

    def _handle_audio_event_ended(self, msg):
        self.ten_env.log_info(f"_handle_audio_event_ended, msg: {msg}")

    def get_words(self, words: List[SpeechmaticsASRWord]) -> List[Word]:
        """
        Get the cached words for sentence final mode.
        """
        new_words = []
        for w in words:
            entry = {
                "word": w.word,
                "start_ms": w.start_ms,
                "duration_ms": w.duration_ms,
                "stable": True,
            }
            if w.speaker is not None:
                entry["speaker"] = w.speaker
            if w.channel is not None:
                entry["channel"] = w.channel
            new_words.append(entry)
        return new_words

    def _convert_to_asr_words(
        self, words: List[SpeechmaticsASRWord], stable: bool
    ) -> List[Word]:
        if not words:
            return []

        converted: List[Word] = []
        for word in words:
            if not word.word:
                continue
            converted.append(
                Word(
                    word=word.word,
                    start_ms=word.start_ms,
                    duration_ms=word.duration_ms,
                    stable=stable,
                    speaker=word.speaker,
                    channel=word.channel,
                )
            )
        return converted

    def _build_diarization_metadata(
        self, words: List[SpeechmaticsASRWord]
    ) -> dict:
        if not words or not self.config.diarization:
            return {}

        segments = get_speaker_segments(words, self.config)
        if not segments:
            return {"diarization": {"mode": self.config.diarization}}

        speakers = sorted({segment["speaker"] for segment in segments})

        metadata: dict = {
            "diarization": {
                "mode": self.config.diarization,
                "segments": segments,
                "speakers": speakers,
            }
        }

        if self.config.channel_diarization_labels:
            metadata["diarization"]["channel_labels"] = (
                self.config.channel_diarization_labels
            )

        return metadata

    async def _emit_error(
        self,
        error: ModuleError,
        vendor_info: Optional[ModuleErrorVendorInfo] = None,
    ):
        """
        Emit an error message to the extension.
        """
        self.ten_env.log_error(
            f"_emit_error, error: {error}, vendor_info: {vendor_info}"
        )
        if callable(self.on_asr_error):
            await self.on_asr_error(
                error, vendor_info
            )  # pylint: disable=not-callable

    def is_connected(self) -> bool:
        if self.client is None:
            return False

        # Check the internal state of the speechmatics client
        try:
            # Check multiple status indicators according to the speechmatics-python library
            session_running = getattr(self.client, "session_running", False)

            # If the client is stopping, consider it as not connected
            if self.client_needs_stopping:
                return False

            if not self.connected:
                return False

            return session_running
        except Exception as e:
            # If an exception occurs while checking the status, consider it as not connected
            self.ten_env.log_debug(f"Error checking connection status: {e}")
            return False

    def get_connection_info(self) -> dict:
        """Get detailed connection information for debugging"""
        info = {
            "client_exists": self.client is not None,
            "client_needs_stopping": self.client_needs_stopping,
            "session_id": self.session_id,
            "audio_queue_size": self.audio_queue.qsize(),
            "running_task_exists": self.client_running_task is not None,
            "is_connected": self.is_connected(),
        }

        if self.client:
            info.update(
                {
                    "session_running": getattr(
                        self.client, "session_running", False
                    ),
                    "client_type": type(self.client).__name__,
                }
            )

        return info
