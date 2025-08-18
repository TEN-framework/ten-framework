import asyncio
from collections.abc import Callable
from time import time
from typing import AsyncGenerator, AsyncIterator
import uuid

from cartesia import AsyncCartesia, WebSocketTtsOutput
from cartesia.tts._async_websocket import AsyncTtsWebsocket

from .config import CartesiaTTSConfig
from ten_runtime import AsyncTenEnv

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_FLUSH = 4


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
        self.client = AsyncCartesia(api_key=self.config.api_key)
        self.ws: AsyncTtsWebsocket | None = None
        self.send_fatal_tts_error = send_fatal_tts_error
        self.send_non_fatal_tts_error = send_non_fatal_tts_error

    async def start(self) -> None:
        """Preheating: establish websocket connection during initialization"""
        try:
            await self._connect()

        except Exception as e:
            self.ten_env.log_error(f"Cartesia TTS preheat failed: {e}")

    async def _connect(self) -> None:
        """Connect to the websocket"""
        try:
            start_time = time()
            self.ws = await self.client.tts.websocket()
            self.ten_env.log_info(
                f"Cartesia websocket connected successfully, took: {time() - start_time}"
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
                    f"Cartesia TTS preheat failed,unexpected error: {e}"
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
            await self.ws.close()

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Generate TTS audio for the given text, returns (audio_data, event_status)"""

        self.ten_env.log_debug(f"KEYPOINT generate_TTS for '{text}' ")

        self._is_cancelled = False
        try:
            await self._ensure_connection()
            # Send TTS request and yield audio chunks with event status
            async for audio_chunk, event_status in self._process_single_tts(
                text
            ):
                yield audio_chunk, event_status

        except Exception as e:
            self.ten_env.log_error(f"Error in TTS get(): {e}")
            raise

    async def _ensure_connection(self) -> None:
        """Ensure websocket connection is established"""
        if not self.ws:
            await self._connect()

    async def _process_single_tts(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Process a single TTS request in serial manner"""
        if not self.ws:
            self.ten_env.log_error("Cartesia websocket not connected")
            return

        self.ten_env.log_info(f"process_single_tts,text:{text}")

        context_id = uuid.uuid4().hex
        output_generator: AsyncGenerator[WebSocketTtsOutput, None] = (
            await self.ws.send(
                transcript=text,
                context_id=context_id,
                stream=True,
                **self.config.params,
            )
        )

        try:
            async for output in output_generator:
                if self._is_cancelled:
                    self.ten_env.log_info(
                        "Cancellation flag detected, sending flush event and stopping TTS stream."
                    )
                    yield None, EVENT_TTS_FLUSH
                    break

                if output.flush_done:
                    self.ten_env.log_debug(
                        f"context_id:{context_id} Received flush_done message"
                    )
                    break
                # Process audio data
                if output.audio:
                    self.ten_env.log_info(
                        f"CartesiaTTS: sending EVENT_TTS_RESPONSE, length: {len(output.audio)}"
                    )
                    yield output.audio, EVENT_TTS_RESPONSE

                else:
                    self.ten_env.log_warn(
                        f"context_id: {context_id},flush_done is None, audio is None,output:{output.model_dump_json()}"
                    )

            if not self._is_cancelled:
                self.ten_env.log_info("CartesiaTTS: sending EVENT_TTS_END")
                yield None, EVENT_TTS_END
        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(f"CartesiaTTS failed:{e}")
            yield error_message.encode("utf-8"), EVENT_TTS_ERROR
