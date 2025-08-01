import traceback
from typing import Awaitable, Callable
from pydantic import BaseModel
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ErrorMessage, ModuleType
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    StatusCode,
    CmdResult,
)

import asyncio
import amazon_transcribe.auth
import amazon_transcribe.client
import amazon_transcribe.handlers
import amazon_transcribe.model
from dataclasses import dataclass


@dataclass
class TranscribeASRConfig(BaseModel):
    region: str = "us-east-1"
    access_key: str = ""
    secret_key: str = ""
    sample_rate: int = 16000
    lang_code: str = "en-US"
    media_encoding: str = "pcm"


class TranscribeASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: TranscribeASRConfig = None
        self.client: amazon_transcribe.client.TranscribeStreamingClient = None
        self.stream: (
            amazon_transcribe.model.StartStreamTranscriptionEventStream
        ) = None
        self.handler_task: asyncio.Task = None
        self.event_handler = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("TranscribeASRExtension on_init")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def start_connection(self) -> None:
        try:
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = TranscribeASRConfig.model_validate_json(config_json)

            if self.config.access_key and self.config.secret_key:
                self.client = amazon_transcribe.client.TranscribeStreamingClient(
                    region=self.config.region,
                    credential_resolver=amazon_transcribe.auth.StaticCredentialResolver(
                        access_key_id=self.config.access_key,
                        secret_access_key=self.config.secret_key,
                    ),
                )
            else:
                self.client = (
                    amazon_transcribe.client.TranscribeStreamingClient(
                        region=self.config.region
                    )
                )

            self.stream = await self.client.start_stream_transcription(
                language_code=self.config.lang_code,
                media_sample_rate_hz=self.config.sample_rate,
                media_encoding=self.config.media_encoding,
            )

            self.event_handler = TranscribeEventHandler(
                self.stream.output_stream, self.ten_env
            )
            self.event_handler.on_transcript_event_cb = self.on_transcript_event
            self.handler_task = asyncio.create_task(
                self.event_handler.handle_events()
            )

            self.ten_env.log_info("Transcribe stream started")

        except Exception as e:
            self.ten_env.log_error(
                f"start_connection error: {traceback.format_exc()}"
            )
            await self.send_asr_error(
                ErrorMessage(
                    code=1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.ASR,
                )
            )
            asyncio.create_task(self._handle_reconnect())

    async def stop_connection(self) -> None:
        if self.stream:
            await self.stream.input_stream.end_stream()
            self.stream = None
        if self.handler_task:
            await self.handler_task
            self.handler_task = None
        self.ten_env.log_info("TranscribeASR connection stopped")

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> None:
        self.session_id = session_id or self.session_id
        frame_buf = frame.get_buf()
        if frame_buf:
            await self.stream.input_stream.send_audio_event(
                audio_chunk=frame_buf
            )

    def is_connected(self) -> bool:
        return self.stream is not None

    async def finalize(self, session_id: str | None) -> None:
        raise NotImplementedError(
            "Finalize method is not implemented in TranscribeASRExtension"
        )

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)
        self.ten_env.log_info("Attempting reconnect...")
        await self.start_connection()

    async def on_transcript_event(
        self, transcript_event: amazon_transcribe.model.TranscriptEvent
    ) -> None:
        try:
            text_result = ""
            is_final = True

            for result in transcript_event.transcript.results:
                if result.is_partial:
                    is_final = False
                for alt in result.alternatives:
                    text_result += alt.transcript

            if not text_result:
                return

            self.ten_env.log_info(
                f"got transcript: [{text_result}], is_final: [{is_final}]"
            )

            transcription = UserTranscription(
                text=text_result,
                final=is_final,
                start_ms=0,
                duration_ms=0,
                language=self.config.lang_code,
                words=[],
                metadata={"session_id": self.session_id},
            )
            await self.send_asr_transcription(transcription)
        except Exception as e:
            self.ten_env.log_error(f"handle_transcript_event error: {e}")


class TranscribeEventHandler(
    amazon_transcribe.handlers.TranscriptResultStreamHandler
):
    def __init__(
        self,
        transcript_result_stream: amazon_transcribe.model.TranscriptResultStream,
        ten_env: AsyncTenEnv,
    ):
        super().__init__(transcript_result_stream)
        self.ten_env = ten_env
        self.on_transcript_event_cb: (
            Callable[[amazon_transcribe.model.TranscriptEvent], Awaitable[None]]
            | None
        ) = None

    async def handle_transcript_event(
        self, transcript_event: amazon_transcribe.model.TranscriptEvent
    ) -> None:
        if self.on_transcript_event_cb:
            await self.on_transcript_event_cb(transcript_event)
        else:
            self.ten_env.log_warn("No handler registered for transcript event.")
