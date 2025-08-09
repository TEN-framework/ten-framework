#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import os
from typing import Awaitable, Callable, List, TYPE_CHECKING, Optional
import speechmatics.models
import speechmatics.client
from ten_ai_base.message import ErrorMessage, ErrorMessageVendorInfo, ModuleType
from ten_ai_base.transcription import UserTranscription, Word
from ten_runtime import AsyncTenEnv, AudioFrame
from .audio_stream import AudioStream, AudioStreamEventType
from .config import SpeechmaticsASRConfig
from .word import (
    SpeechmaticsASRWord,
    convert_words_to_sentence,
    get_sentence_duration_ms,
    get_sentence_start_ms,
)
from .timeline import AudioTimeline
from .language_utils import get_speechmatics_language
from .dumper import Dumper

if TYPE_CHECKING:
    from .extension import SpeechmaticsASRExtension  # Only for type hints


async def run_asr_client(client: "SpeechmaticsASRClient"):
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
    ):
        self.config = config
        self.ten_env = ten_env
        self.task = None
        self.audio_queue = asyncio.Queue()
        self.timeline = AudioTimeline()
        self.audio_stream = AudioStream(
            self.audio_queue, self.config, self.timeline, ten_env
        )
        self.client_running_task: asyncio.Task = None
        self.client_needs_stopping = False
        self.sent_user_audio_duration_ms_before_last_reset = 0
        self.last_drain_timestamp: int = 0
        self.session_id = None

        # Cache the words for sentence final mode
        self.cache_words = []  # type: List[SpeechmaticsASRWord]

        if self.config.dump:
            dump_file_path = os.path.join(
                self.config.dump_path, "speechmatics_asr_in.pcm"
            )
            self.audio_dumper = Dumper(dump_file_path)

        self.audio_settings: speechmatics.models.AudioSettings | None = None
        self.transcription_config: (
            speechmatics.models.TranscriptionConfig | None
        ) = None
        self.client: speechmatics.client.WebsocketClient | None = None
        self.on_transcription: Optional[
            Callable[[UserTranscription], Awaitable[None]]
        ] = None
        self.on_error: Optional[
            Callable[
                [ErrorMessage, Optional[ErrorMessageVendorInfo]],
                Awaitable[None],
            ]
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
            chunk_size=chunk_len,
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
        self.transcription_config = speechmatics.models.TranscriptionConfig(
            enable_partials=self.config.enable_partials,
            language=get_speechmatics_language(self.config.language),
            max_delay=self.config.max_delay,
            max_delay_mode=self.config.max_delay_mode,
            additional_vocab=additional_vocab,
            operating_point=(
                self.config.operating_point
                if self.config.operating_point
                else None
            ),
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

        if self.config.dump:
            await self.audio_dumper.start()

    async def recv_audio_frame(
        self, frame: AudioFrame, session_id: str
    ) -> None:
        frame_buf = frame.get_buf()
        if not frame_buf:
            self.ten_env.log_warn("send_frame: empty audio_frame detected.")
            return

        self.session_id = session_id

        try:
            await self.audio_queue.put(frame_buf)
            if self.config.dump:
                await self.audio_dumper.push_bytes(frame_buf)
        except Exception as e:
            self.ten_env.log_error(f"Error sending audio frame: {e}")
            error = ErrorMessage(
                code=1,
                message=str(e),
                turn_id=0,
                module=ModuleType.ASR,
            )
            asyncio.create_task(self._emit_error(error, None))

    async def stop(self) -> None:
        self.ten_env.log_info("call stop")
        self.client_needs_stopping = True

        self.client.stop()

        await self.audio_queue.put(AudioStreamEventType.FLUSH)
        await self.audio_queue.put(AudioStreamEventType.CLOSE)

        if self.client_running_task:
            await self.client_running_task

        self.client_running_task = None

        if self.config.dump:
            await self.audio_dumper.stop()

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
                error_message = ErrorMessage(
                    code=-1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.ASR,
                )
                asyncio.create_task(self._emit_error(error_message, None))

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
        self.ten_env.log_info(f"_handle_recognition_started, msg: {msg}")
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.timeline.get_total_user_audio_duration()
        )
        self.timeline.reset()

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

            transcription = UserTranscription(
                text=text,
                final=False,
                start_ms=_actual_start_ms,
                duration_ms=_duration_ms,
                language=self.config.language,
                words=[],
                metadata={
                    "session_id": self.session_id,
                },
            )

            if self.on_transcription:
                asyncio.create_task(self.on_transcription(transcription))
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error_message = ErrorMessage(
                code=1,
                message=str(e),
                turn_id=0,
                module=ModuleType.ASR,
            )

            asyncio.create_task(self._emit_error(error_message, None))

    def _handle_transcript_word_final_mode(self, msg):
        try:
            metadata = msg.get("metadata", {})
            text = metadata.get("transcript", "")
            if text:
                start_ms = metadata.get("start_time", 0) * 1000
                end_ms = metadata.get("end_time", 0) * 1000
                _duration_ms = int(end_ms - start_ms)
                _actual_start_ms = int(
                    self.timeline.get_audio_duration_before_time(start_ms)
                    + self.sent_user_audio_duration_ms_before_last_reset
                )

                transcription = UserTranscription(
                    text=text,
                    final=True,
                    start_ms=_actual_start_ms,
                    duration_ms=_duration_ms,
                    language=self.config.language,
                    words=[],
                    metadata={
                        "session_id": self.session_id,
                    },
                )

                if self.on_transcription:
                    asyncio.create_task(self.on_transcription(transcription))
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error_message = ErrorMessage(
                code=1,
                message=str(e),
                turn_id=0,
                module=ModuleType.ASR,
            )

            asyncio.create_task(self._emit_error(error_message, None))

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

                        word = SpeechmaticsASRWord(
                            word=text,
                            start_ms=actual_start_ms,
                            duration_ms=duration_ms,
                            is_punctuation=is_punctuation,
                        )
                        self.cache_words.append(word)

                if result.get("is_eos") == True:
                    sentence = convert_words_to_sentence(
                        self.cache_words, self.config
                    )
                    start_ms = get_sentence_start_ms(self.cache_words)
                    duration_ms = get_sentence_duration_ms(self.cache_words)

                    user_transcription = UserTranscription(
                        text=sentence,
                        final=True,
                        start_ms=start_ms,
                        duration_ms=duration_ms,
                        language=self.config.language,
                        words=self.get_words(self.cache_words),
                        metadata={
                            "session_id": self.session_id,
                        },
                    )

                    if self.on_transcription:
                        asyncio.create_task(
                            self.on_transcription(user_transcription)
                        )
                    self.cache_words = []

            # if the transcript is not empty, send it as a partial transcript
            if self.cache_words:
                sentence = convert_words_to_sentence(
                    self.cache_words, self.config
                )
                start_ms = get_sentence_start_ms(self.cache_words)
                duration_ms = get_sentence_duration_ms(self.cache_words)

                user_transcription = UserTranscription(
                    text=sentence,
                    final=False,
                    start_ms=start_ms,
                    duration_ms=duration_ms,
                    language=self.config.language,
                    words=self.get_words(self.cache_words),
                    metadata={
                        "session_id": self.session_id,
                    },
                )

                if self.on_transcription:
                    asyncio.create_task(
                        self.on_transcription(user_transcription)
                    )
        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")
            error_message = ErrorMessage(
                code=1,
                message=str(e),
                turn_id=0,
                module=ModuleType.ASR,
            )

            asyncio.create_task(self._emit_error(error_message, None))

    def _handle_end_transcript(self, msg):
        self.ten_env.log_info(f"_handle_end_transcript, msg: {msg}")

    def _handle_info(self, msg):
        self.ten_env.log_info(f"_handle_info, msg: {msg}")

    def _handle_warning(self, msg):
        self.ten_env.log_warn(f"_handle_warning, msg: {msg}")

    def _handle_error(self, error):
        self.ten_env.log_error(f"_handle_error, error: {error}")
        error_message = ErrorMessage(
            code=-1,
            message=str(error),
            turn_id=0,
            module=ModuleType.ASR,
        )

        asyncio.create_task(
            self._emit_error(
                error_message,
                {
                    "vendor": "speechmatics",
                    "code": error.code if hasattr(error, "code") else -1,
                    "message": str(error),
                },
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
            new_words.append(
                {
                    "word": w.word,
                    "start_ms": w.start_ms,
                    "duration_ms": w.duration_ms,
                    "stable": True,
                }
            )
        return new_words

    async def _emit_error(
        self,
        error_message: ErrorMessage,
        vendor_info: Optional[ErrorMessageVendorInfo] = None,
    ):
        """
        Emit an error message to the extension.
        """
        self.ten_env.log_error(f"Error: {error_message.message}")
        if callable(self.on_error):
            await self.on_error(  # pylint: disable=not-callable
                error_message, vendor_info
            )
