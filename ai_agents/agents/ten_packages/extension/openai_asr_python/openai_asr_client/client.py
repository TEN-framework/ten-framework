import asyncio
import base64
import json
import logging
import os
import urllib.parse
from typing import Any, Callable

from openai import OpenAIError
from typing_extensions import override

from .log import get_logger
from .schemas import (
    Error,
    Session,
    TranscriptionParam,
    TranscriptionResultCommitted,
    TranscriptionResultCompleted,
    TranscriptionResultDelta,
)
from .ws_client import WebSocketClient


class AsyncOpenAIAsrListener:
    async def on_asr_start(self, response: Session[TranscriptionParam]):
        pass

    async def on_asr_server_error(self, response: Session[Error]):
        """
        server error.
        """
        pass

    async def on_asr_client_error(
        self, response: Any, error: Exception | None = None
    ):
        """
        client capture the error.
        """
        pass

    async def on_asr_delta(self, response: TranscriptionResultDelta):
        """
        delta of the transcription.
        """
        pass

    async def on_asr_completed(self, response: TranscriptionResultCompleted):
        """
        completed of the transcription.
        """
        pass

    async def on_asr_committed(self, response: TranscriptionResultCommitted):
        """
        committed of the transcription.
        """
        pass

    async def on_other_event(self, response: dict):
        """
        other event.
        """
        pass


class OpenAIAsrClient(WebSocketClient):
    def __init__(
        self,
        params: TranscriptionParam,
        api_key: str | None = None,
        organization: str | None = None,
        project: str | None = None,
        websocket_base_url: str | None = None,
        logger: logging.Logger | None = None,
        log_level: str = "INFO",
        log_path: str | None = None,
        listener: AsyncOpenAIAsrListener | None = None,
        **kwargs,
    ):
        if api_key is None:
            api_key = os.environ.get("OPENAI_API_KEY")
        if api_key is None:
            raise OpenAIError(
                "The api_key client option must be set either by passing api_key to the client or by setting the OPENAI_API_KEY environment variable"
            )
        self.api_key = api_key

        if organization is None:
            organization = os.environ.get("OPENAI_ORG_ID")
        self.organization = organization

        if project is None:
            project = os.environ.get("OPENAI_PROJECT_ID")
        self.project = project

        if websocket_base_url is None:
            websocket_base_url = os.environ.get("OPENAI_WEBSOCKET_BASE_URL")
        if websocket_base_url is None:
            websocket_base_url = "wss://api.openai.com/v1/"
        self.websocket_base_url = websocket_base_url

        if logger is None:
            self.logger = get_logger(level=log_level, log_path=log_path)
        else:
            self.logger = logger

        if listener is None:
            self._listener = AsyncOpenAIAsrListener()
        else:
            self._listener = listener

        self._params = params

        query_params = {
            "intent": "transcription",
        }
        end_point = urllib.parse.urljoin(websocket_base_url, "realtime")

        end_point += "?" + urllib.parse.urlencode(query_params)

        kwargs["additional_headers"] = [
            ("Authorization", f"Bearer {self.api_key}"),
            ("OpenAI-Beta", "realtime=v1"),
        ]
        if self.organization:
            kwargs["additional_headers"].append(
                ("OpenAI-Organization", self.organization)
            )
        if self.project:
            kwargs["additional_headers"].append(("OpenAI-Project", self.project))

        # for beta realtime api, we must connect with the server first,
        # then send the transcription session update param to the server.
        # so we need to wait for the server to be ready.
        self.params_ready_event = asyncio.Event()

        super().__init__(end_point, logger=self.logger, **kwargs)

    async def _call_listener(self, func: Callable, *args, **kwargs):
        # awaitable function
        if asyncio.iscoroutinefunction(func):
            await func(*args, **kwargs)
        else:
            func(*args, **kwargs)
    
    async def _update_session(self):
        self.params_ready_event.clear()
        session = Session[TranscriptionParam](
            type="transcription_session.update",
            event_id=None,
            session=self._params,
        )
        await self.send(session.model_dump_json(exclude_none=True))
    
    async def _handle_event(self, message: dict):
        _type = message.get("type")
        if _type == "transcription_session.updated":
            self.params_ready_event.set()
            await self._call_listener(self._listener.on_asr_start, Session[TranscriptionParam](
                type="transcription_session.update",
                event_id=None,
                session=self._params,
            ))
            return
        elif _type == "conversation.item.input_audio_transcription.delta":
            await self._call_listener(self._listener.on_asr_delta, TranscriptionResultDelta.model_validate(message))
            return
        elif _type == "conversation.item.input_audio_transcription.completed":
            await self._call_listener(self._listener.on_asr_completed, TranscriptionResultCompleted.model_validate(message))
            return
        elif _type == "input_audio_buffer.committed":
            await self._call_listener(self._listener.on_asr_committed, TranscriptionResultCommitted.model_validate(message))
            return
        else:
            await self._call_listener(self._listener.on_other_event, message)
            return
    
    async def _handle_error(self, message: Session[Error]):
        if not self.params_ready_event.is_set() and message.session.type == "invalid_request_error":
            # params invalid, call the server error listener then stop the client
            await self._call_listener(self._listener.on_asr_server_error, message)
            await self.stop()
            return

    @override
    async def on_open(self):
        await self._update_session()

    @override
    async def on_message(self, message: str | bytes):
        self.logger.debug(f"🔄 Received message: {message}")
        try:
            message = json.loads(message)
        except Exception as e:
            # unexpected message, call the client error listener
            msg = f"💥 An error occurred to parse message: {message}"
            self.logger.error(msg)
            await self._call_listener(self._listener.on_asr_client_error, msg, e)
            await self.stop()
            return
        assert isinstance(message, dict), f"message is not a dict: {message}"

        _type = message.get("type")
        if _type is None:
            self.logger.error(f"💥 An error occurred. unknown message type: {message}")
            # ignore the error, just return
            return
        
        if _type == "error":
            await self._handle_error(Session[Error](
                type="error",
                event_id=message.get("event_id"),
                session=Error.model_validate(message.get("error")),
            ))
            return
        await self._handle_event(message)

    @override
    async def on_close(self, code: int, reason: str):
        self.logger.warning(f"🔴 Connection closed. Code: {code}, Reason: {reason}")

    @override
    async def on_error(self, error: Exception):
        self.logger.error(f"💥 An error occurred: {error}")
        await self._call_listener(self._listener.on_asr_client_error, str(error), error)
        return

    @override
    async def on_reconnect(self):
        self.logger.info("🔄 Try to reconnect to the server.")
        self.params_ready_event.clear()
        return

    async def send_pcm_data(self, data: bytes):
        base64_data = base64.b64encode(data).decode("utf-8")
        await self.send(
            json.dumps({"type": "input_audio_buffer.append", "audio": base64_data})
        )

    async def send_end_of_stream(self):
        await self.send(json.dumps({"type": "input_audio_buffer.commit"}))

    async def send_heartbeat(self):
        await self.send(b"")

    def is_ready(self):
        return self.params_ready_event.is_set()


if __name__ == "__main__":
    import os
    from pathlib import Path

    async def send_audio_data(client: OpenAIAsrClient):
        with open(
            Path(__file__).parent.parent / "tests/test_data/16k_zh_CN.pcm",
            "rb",
        ) as f:
            sample_rate = 16000
            if sample_rate is None:
                sample_rate = 16000
            total_ms = 20000
            chunk_time_ms = 50
            chunk_size = int(chunk_time_ms * sample_rate / 1000 * 2)
            cnt = 0
            while not client.is_ready():
                await asyncio.sleep(0.1)
            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    await client.send_end_of_stream()
                    break
                await client.send_pcm_data(chunk)
                await asyncio.sleep(chunk_time_ms / 1000)
                cnt += chunk_time_ms
                if cnt > total_ms:
                    await client.send_end_of_stream()
                    break

    async def main():
        params = TranscriptionParam(
            input_audio_format="pcm16",
            input_audio_transcription={
                "model": "whisper-1",
                "prompt": "Please transcribe the following audio into text. 输出简体中文。热词：山口茜、戴资颖",
                "language": "zh",
            },
            turn_detection={
                "type": "server_vad",
            },
            input_audio_noise_reduction={
                "type": "near_field",
            },
            include=[
                # "item.input_audio_transcription.logprobs"
            ],
        )
        client = OpenAIAsrClient(
            params=params,
            log_level="DEBUG",
            auto_reconnect=True,
        )
        logger = client.logger

        try:
            asyncio.create_task(send_audio_data(client))
            await client.start()
        except KeyboardInterrupt:
            logger.info("Keyboard interrupt received.")
        finally:
            logger.info("Main is shutting down the client...")
            await client.stop()

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
