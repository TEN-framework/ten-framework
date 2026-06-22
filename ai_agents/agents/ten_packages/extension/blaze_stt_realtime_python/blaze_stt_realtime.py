"""
Blaze Realtime STT Extension Implementation

This extension wraps the Blaze realtime STT WebSocket endpoint for use in the
TEN framework.

Protocol (WebSocket: /v1/stt/realtime):
    1. Connect to ws(s)://<host>/v1/stt/realtime
    2. Send a JSON init message: {"token", "language", "model", "enable_log"}
    3. Receive {"type": "ready"} once the upstream session is established
    4. Stream binary PCM audio chunks (16 kHz, mono, 16-bit little-endian)
    5. Receive transcript messages:
        - {"type": "partial", "text": "..."}  interim result
        - {"type": "final",   "text": "..."}  stable result
        - {"type": "error",   "text": "..."}  error
    6. Close the connection to signal end-of-audio.
"""

import asyncio
import json
import logging
import os
from typing import (
    Any,
    AsyncIterable,
    AsyncIterator,
    Dict,
    Iterable,
    List,
    Optional,
    Union,
)

import websockets
from pydantic import BaseModel, Field

logger = logging.getLogger(__name__)


class BlazeRealtimeClientConfig(BaseModel):
    """Configuration for the Blaze realtime STT websocket client."""

    api_url: str = Field(
        default_factory=lambda: os.getenv(
            "BLAZE_STT_API_URL", "http://localhost:8000"
        ),
        description="Blaze STT API base URL (http/https; converted to ws/wss)",
    )
    api_key: Optional[str] = Field(
        default_factory=lambda: os.getenv("BLAZE_STT_API_KEY"),
        description="API token sent in the init message",
    )
    timeout: int = Field(
        default=3600, description="Overall session timeout in seconds"
    )
    default_language: str = Field(
        default="vi",
        description="Default language code (e.g. 'vi' for Vietnamese)",
    )
    default_model: str = Field(
        default="stt-stream-1.5",
        description="Default realtime STT model",
    )
    sample_rate: int = Field(
        default=16000,
        description="Expected PCM sample rate (16 kHz, mono, 16-bit)",
    )
    enable_log: bool = Field(
        default=False,
        description="Ask the server to persist a session log",
    )


class BlazeRealtimeClient:
    """
    Thin async websocket client for the Blaze realtime STT endpoint.

    Wraps the realtime STT WebSocket endpoint: /v1/stt/realtime

    Provides a streaming transcribe_stream() helper plus a buffered
    transcribe() convenience wrapper. This is the transport layer used by the
    TEN ``BlazeSTTRealtimeExtension`` (see extension.py); it is intentionally
    free of any TEN runtime dependency so it can be unit-tested standalone.
    """

    def __init__(
        self,
        config: Optional[
            Union[BlazeRealtimeClientConfig, Dict[str, Any]]
        ] = None,
    ):
        """
        Initialize the Blaze realtime STT client.

        Args:
            config: Configuration object (BlazeRealtimeClientConfig) or dict
                    from the TEN framework. If None, uses environment variables.
        """
        if config is None:
            self.config = BlazeRealtimeClientConfig()
        elif isinstance(config, dict):
            # Convert dict from TEN framework to BlazeRealtimeClientConfig
            self.config = BlazeRealtimeClientConfig(
                api_url=config.get("api_url", "http://localhost:8000"),
                api_key=config.get("api_key"),
                default_language=config.get("language", "vi"),
                default_model=config.get("model", "stt-stream-1.5"),
                timeout=config.get("timeout", 3600),
                sample_rate=config.get("sample_rate", 16000),
                enable_log=config.get("enable_log", False),
            )
        else:
            self.config = config

        self.base_url = self.config.api_url.rstrip("/")
        self.ws_url = self.build_ws_url(self.base_url)

        logger.info(
            f"Blaze Realtime STT Extension initialized with WS URL: "
            f"{self.ws_url}"
        )

    @staticmethod
    def build_ws_url(base_url: str) -> str:
        """Convert an http(s) base URL into the realtime ws(s) endpoint."""
        if base_url.startswith("https://"):
            ws_base = "wss://" + base_url[len("https://") :]
        elif base_url.startswith("http://"):
            ws_base = "ws://" + base_url[len("http://") :]
        else:
            # Assume the URL is already a ws/wss URL (or scheme-less host)
            ws_base = base_url
        return f"{ws_base}/v1/stt/realtime"

    async def transcribe_stream(
        self,
        audio_chunks: Union[Iterable[bytes], AsyncIterable[bytes]],
        language: Optional[str] = None,
        model: Optional[str] = None,
        enable_log: Optional[bool] = None,
        drain_timeout: float = 2.0,
    ) -> AsyncIterator[Dict[str, Any]]:
        """
        Stream audio to the realtime endpoint and yield transcript events.

        Args:
            audio_chunks: Iterable (sync or async) of binary PCM chunks
                          (16 kHz, mono, 16-bit little-endian).
            language: Language code. Defaults to config default.
            model: Realtime model. Defaults to config default.
            enable_log: Ask the server to persist a session log.
                        Defaults to config value.
            drain_timeout: Seconds to keep receiving trailing transcripts
                           after all audio has been sent before closing.

        Yields:
            Event dicts, e.g.
                {"type": "ready"}
                {"type": "partial", "text": "..."}
                {"type": "final", "text": "..."}
                {"type": "error", "text": "..."}

        Raises:
            websockets.WebSocketException: On connection/protocol errors.
            ValueError: If the server reports an error before "ready".
        """
        language = language or self.config.default_language
        model = model or self.config.default_model
        enable_log = (
            self.config.enable_log if enable_log is None else enable_log
        )

        init_message = {
            "token": self.config.api_key,
            "language": language,
            "model": model,
            "enable_log": enable_log,
        }

        async with websockets.connect(
            self.ws_url,
            max_size=None,
            open_timeout=self.config.timeout,
            close_timeout=drain_timeout,
        ) as ws:
            await ws.send(json.dumps(init_message))

            # Wait for the "ready" signal before streaming audio.
            ready = False
            while not ready:
                raw = await ws.recv()
                if isinstance(raw, bytes):
                    continue
                msg = json.loads(raw)
                if msg.get("type") == "ready":
                    ready = True
                    yield msg
                elif msg.get("type") == "error":
                    raise ValueError(f"Realtime STT error: {msg.get('text')}")
                else:
                    # Forward any pre-ready messages to the caller.
                    yield msg

            sender_done = asyncio.Event()

            async def _send_audio() -> None:
                try:
                    if hasattr(audio_chunks, "__aiter__"):
                        async for chunk in audio_chunks:  # type: ignore
                            if chunk:
                                await ws.send(chunk)
                    else:
                        for chunk in audio_chunks:  # type: ignore
                            if chunk:
                                await ws.send(chunk)
                finally:
                    sender_done.set()

            sender_task = asyncio.create_task(_send_audio())

            try:
                while True:
                    try:
                        # While audio is still being sent, block on recv.
                        # Once sending is done, drain trailing transcripts
                        # with a bounded timeout.
                        if sender_done.is_set():
                            raw = await asyncio.wait_for(
                                ws.recv(), timeout=drain_timeout
                            )
                        else:
                            raw = await ws.recv()
                    except asyncio.TimeoutError:
                        # No more transcripts arrived during the drain window.
                        break
                    except websockets.ConnectionClosed:
                        break

                    if isinstance(raw, bytes):
                        continue
                    yield json.loads(raw)
            finally:
                if not sender_task.done():
                    sender_task.cancel()
                    try:
                        await sender_task
                    except (asyncio.CancelledError, Exception):
                        pass

    async def transcribe(
        self,
        audio_data: bytes,
        language: Optional[str] = None,
        model: Optional[str] = None,
        enable_log: Optional[bool] = None,
        chunk_size: int = 3200,
        chunk_interval: float = 0.1,
        drain_timeout: float = 2.0,
    ) -> Dict[str, Any]:
        """
        Transcribe a complete PCM buffer by streaming it in chunks.

        This is a convenience wrapper around transcribe_stream() that splits a
        full PCM buffer into chunks, paces them to approximate realtime, and
        accumulates the resulting transcript.

        Args:
            audio_data: PCM audio (16 kHz, mono, 16-bit little-endian).
            language: Language code. Defaults to config default.
            model: Realtime model. Defaults to config default.
            enable_log: Ask the server to persist a session log.
            chunk_size: Bytes per chunk (3200 bytes ~= 100 ms at 16 kHz/16-bit).
            chunk_interval: Seconds to wait between chunks (real-time pacing).
            drain_timeout: Seconds to wait for trailing finals before closing.

        Returns:
            Dict with:
                - transcription (str): Concatenated final transcript text
                - finals (List[str]): All final segments
                - partials (List[str]): All partial segments
                - events (List[dict]): Raw event stream
        """
        if not audio_data:
            raise ValueError("audio_data cannot be empty")

        async def _chunked() -> AsyncIterator[bytes]:
            for i in range(0, len(audio_data), chunk_size):
                yield audio_data[i : i + chunk_size]
                if chunk_interval:
                    await asyncio.sleep(chunk_interval)

        finals: List[str] = []
        partials: List[str] = []
        events: List[Dict[str, Any]] = []

        async for event in self.transcribe_stream(
            _chunked(),
            language=language,
            model=model,
            enable_log=enable_log,
            drain_timeout=drain_timeout,
        ):
            events.append(event)
            etype = event.get("type")
            if etype == "final":
                text = event.get("text") or ""
                if text:
                    finals.append(text)
            elif etype == "partial":
                text = event.get("text") or ""
                if text:
                    partials.append(text)
            elif etype == "error":
                logger.error(f"Realtime STT error event: {event.get('text')}")

        return {
            "transcription": " ".join(finals).strip(),
            "finals": finals,
            "partials": partials,
            "events": events,
        }

    async def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process input according to the TEN framework interface.

        Args:
            input_data: Input dict with:
                - audio_data (bytes): Required. PCM audio to transcribe.
                - language (str): Optional. Language code.
                - model (str): Optional. Realtime model.
                - enable_log (bool): Optional. Persist a session log.

        Returns:
            Output dict with:
                - transcription (str): Transcribed text
                - status (str): "completed"
                - raw_result (dict): Full result from transcribe()
        """
        audio_data = input_data.get("audio_data")
        if not audio_data:
            raise ValueError("audio_data is required in input_data")

        result = await self.transcribe(
            audio_data=audio_data,
            language=input_data.get("language"),
            model=input_data.get("model"),
            enable_log=input_data.get("enable_log"),
        )

        return {
            "transcription": result.get("transcription", ""),
            "status": "completed",
            "raw_result": result,
        }

    def get_metadata(self) -> Dict[str, Any]:
        """
        Return extension metadata for the TEN framework.

        Returns:
            Dict with extension information.
        """
        return {
            "name": "blaze_stt_realtime_python",
            "version": "1.0.0",
            "description": (
                "Blaze realtime Speech-to-Text extension for the TEN framework"
            ),
            "capabilities": [
                "stt",
                "asr",
                "realtime",
                "streaming",
                "transcription",
                "speech_to_text",
            ],
            "transport": "websocket",
            "endpoint": "/v1/stt/realtime",
            "audio_format": {
                "encoding": "pcm_s16le",
                "sample_rate": self.config.sample_rate,
                "num_channels": 1,
            },
            "supported_languages": ["vi", "en"],
            "config_schema": {
                "api_url": {
                    "type": "string",
                    "required": False,
                    "default": "http://localhost:8000",
                },
                "api_key": {"type": "string", "required": False},
                "language": {
                    "type": "string",
                    "required": False,
                    "default": "vi",
                },
                "model": {
                    "type": "string",
                    "required": False,
                    "default": "stt-stream-1.5",
                },
            },
        }
