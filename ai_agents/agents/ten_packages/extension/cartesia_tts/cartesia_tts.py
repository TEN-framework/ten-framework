import asyncio
from collections.abc import Callable
from datetime import datetime
from typing import AsyncIterator, Any
import uuid

from cartesia import AsyncCartesia  # pylint: disable=no-name-in-module

from .config import CartesiaTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_FLUSH = 4
EVENT_TTS_TTFB_METRIC = 5


class CartesiaTTSConnectionException(Exception):
    """Exception raised when Cartesia TTS connection fails"""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Cartesia TTS connection failed (code: {status_code}): {body}"
        )


class CartesiaTTSClient:
    def __init__(
        self,
        config: CartesiaTTSConfig,
        ten_env: AsyncTenEnv,
        send_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
        send_non_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
    ):
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        if self.config.base_url.strip() != "":
            self.client = AsyncCartesia(
                api_key=self.config.api_key, base_url=self.config.base_url
            )
        else:
            self.client = AsyncCartesia(api_key=self.config.api_key)
        self.ws: Any = None  # WebSocket connection object
        self.send_fatal_tts_error = send_fatal_tts_error
        self.send_non_fatal_tts_error = send_non_fatal_tts_error

        self.sent_ts: datetime | None = None
        self.ttfb_sent: bool = False

    async def start(self) -> None:
        """Preheating: establish websocket connection during initialization"""
        try:
            await self._connect()

        except Exception as e:
            self.ten_env.log_error(f"Cartesia TTS preheat failed: {e}")

    async def _connect(self) -> None:
        """Connect to the websocket"""
        try:
            self.ws = await self.client.tts.websocket()
            self.ten_env.log_debug(
                "vendor_status: connected to cartesia tts",
                category=LOG_CATEGORY_VENDOR,
            )

        except Exception as e:
            error_message = str(e)
            if "401" in error_message and "Unauthorized" in error_message:
                if self.send_fatal_tts_error:
                    await self.send_fatal_tts_error(error_message=error_message)
                else:
                    raise CartesiaTTSConnectionException(
                        status_code=401, body=error_message
                    ) from e
            else:
                self.ten_env.log_error(
                    f"Cartesia TTS preheat failed, unexpected error: {e}"
                )
                if self.send_non_fatal_tts_error:
                    await self.send_non_fatal_tts_error(
                        error_message=error_message
                    )
                raise

    async def stop(self):
        # Stop the websocket connection if it exists
        if self.ws:
            await self.ws.close()
            self.ws = None

    async def cancel(self):
        """
        Cancel the current TTS task by closing the websocket connection.
        This will trigger a ConnectionClosed exception in the processing loop.
        """
        self.ten_env.log_debug(
            "Cancelling current TTS task by closing websocket."
        )
        self._is_cancelled = True
        if self.ws:
            self.reset_ttfb()

    def reset_ttfb(self):
        self.sent_ts = None
        self.ttfb_sent = False

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int | None]]:
        """Generate TTS audio for the given text, returns (audio_data, event_status)"""

        self._is_cancelled = False
        try:
            await self._ensure_connection()
            # Send TTS request and yield audio chunks with event status
            async for audio_chunk, event_status in self._process_single_tts(
                text
            ):
                if event_status == EVENT_TTS_FLUSH:
                    await self.ws.close()
                    self.ws = None
                    await self._ensure_connection()

                yield audio_chunk, event_status

        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: {e}", category=LOG_CATEGORY_VENDOR
            )
            raise

    async def _ensure_connection(self) -> None:
        """Ensure websocket connection is established"""
        if not self.ws:
            await self._connect()

    async def _process_single_tts(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int | None]]:
        """Process a single TTS request in serial manner"""
        if not self.ws:
            self.ten_env.log_error("Cartesia websocket not connected")
            return

        self.ten_env.log_debug(f"process_single_tts, text: {text}")

        context_id = uuid.uuid4().hex
        if not self.ttfb_sent:
            self.sent_ts = datetime.now()

        # In SDK v2.0+, ws.send() returns an async generator directly
        output_generator = await self.ws.send(
            transcript=text,
            context_id=context_id,
            stream=True,
            **self.config.params,
        )

        try:
            async for output in output_generator:
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        "Cancellation flag detected, sending flush event and stopping TTS stream."
                    )
                    yield None, EVENT_TTS_FLUSH
                    break

                # Check for flush_done attribute
                if hasattr(output, "flush_done") and output.flush_done:
                    self.ten_env.log_debug(
                        f"context_id: {context_id} Received flush_done message"
                    )
                    break

                # Check for type attribute (new SDK format)
                if hasattr(output, "type") and output.type == "flush_done":
                    self.ten_env.log_debug(
                        f"context_id: {context_id} Received flush_done message (type)"
                    )
                    break

                # Process audio data
                audio_data = None
                if hasattr(output, "audio") and output.audio:
                    audio_data = output.audio
                elif hasattr(output, "data") and output.data:
                    # Some SDK versions use 'data' instead of 'audio'
                    audio_data = output.data

                if audio_data:
                    # First audio chunk for a session, calculate and send TTFB
                    if self.sent_ts and not self.ttfb_sent:
                        ttfb_ms = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        yield ttfb_ms, EVENT_TTS_TTFB_METRIC
                        self.ttfb_sent = True
                    self.ten_env.log_debug(
                        f"CartesiaTTS: sending EVENT_TTS_RESPONSE, length: {len(audio_data)}"
                    )
                    yield audio_data, EVENT_TTS_RESPONSE
                else:
                    # Log output for debugging
                    if hasattr(output, "model_dump_json"):
                        self.ten_env.log_warn(
                            f"context_id: {context_id}, no audio data, output: {output.model_dump_json()}"
                        )
                    else:
                        self.ten_env.log_warn(
                            f"context_id: {context_id}, no audio data, output: {output}"
                        )

            if not self._is_cancelled:
                self.ten_env.log_debug("CartesiaTTS: sending EVENT_TTS_END")
                yield None, EVENT_TTS_END
        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield error_message.encode("utf-8"), EVENT_TTS_ERROR
