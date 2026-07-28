from datetime import datetime
import json
import os
import asyncio
from urllib.parse import urlencode
from typing import Optional

import aiohttp
from typing_extensions import override

from .const import (
    DUMP_FILE_NAME,
    MODULE_NAME_ASR,
    SOURCE_NAME,
)
from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from .config import SmallestASRConfig
from .reconnect_manager import ReconnectManager


class SmallestASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.ws: Optional[aiohttp.ClientWebSocketResponse] = None
        self.session: Optional[aiohttp.ClientSession] = None
        self.config: SmallestASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0
        self._utterance_start_ms: Optional[int] = None
        self.reconnect_manager: ReconnectManager | None = None

        self._message_task: Optional[asyncio.Task] = None
        # In-flight reconnection tasks. Reconnection is always run on its own
        # task (never awaited inline) so it cannot cancel the caller; see
        # `_schedule_reconnect`. Tracked here so tasks are not garbage
        # collected mid-flight and can be cancelled on shutdown.
        self._reconnect_tasks: set[asyncio.Task] = set()

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None
        # Cancel any in-flight reconnection before tearing down the connection
        # so a pending reconnect cannot re-open the socket during shutdown.
        await self._cancel_reconnect_tasks()
        await self.stop_connection()

    async def _cancel_reconnect_tasks(self) -> None:
        """Cancel and drain any outstanding reconnection tasks."""
        tasks = list(self._reconnect_tasks)
        for task in tasks:
            task.cancel()
        for task in tasks:
            try:
                await task
            except asyncio.CancelledError:
                pass
        self._reconnect_tasks.clear()

    @override
    def vendor(self) -> str:
        """Get the name of the ASR vendor."""
        return "smallest"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = SmallestASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            api_key = self.config.api_key or self.config.params.get(
                "api_key", ""
            )
            if not api_key:
                raise ValueError(
                    "Smallest AI API key is required. Provide it in params.api_key or set SMALLEST_API_KEY environment variable."
                )

            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = SmallestASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    def _build_websocket_url(self) -> str:
        """Build WebSocket URL with parameters."""
        assert self.config is not None
        ws_url = self.config.get_ws_url()

        params = {
            "model": self.config.model,
            "language": self.config.wire_language(),
            "encoding": self.config.encoding,
            "sample_rate": self.config.sample_rate,
            # Word timings are used to compute accurate start_ms/duration_ms
            # on final transcripts; can be overridden via params.
            "word_timestamps": "true",
        }

        # Forward extra vendor params (word_timestamps, eou_timeout, ...)
        # excluding first-class fields and secrets.
        for key, value in self.config.params.items():
            if not self.config.is_black_list_params(key):
                params[key] = value

        return f"{ws_url}?{urlencode(params)}"

    @override
    async def start_connection(self) -> None:
        assert self.config is not None
        self.ten_env.log_info("start_connection")

        try:
            await self.stop_connection()

            # Create aiohttp session if not exists
            if self.session is None or self.session.closed:
                self.session = aiohttp.ClientSession()

            if self.audio_dumper:
                await self.audio_dumper.start()

            # Build WebSocket URL
            ws_url = self._build_websocket_url()
            # Get API key from config or params
            api_key = self.config.api_key or self.config.params.get(
                "api_key", ""
            )
            headers = {
                "Authorization": f"Bearer {api_key}",
                "X-Source": SOURCE_NAME,
            }

            self.ten_env.log_info(
                f"Connecting to Smallest AI WebSocket: {ws_url}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Connect to WebSocket
            try:
                timeout = aiohttp.ClientTimeout(total=30)
                self.ws = await asyncio.wait_for(
                    self.session.ws_connect(
                        ws_url, headers=headers, timeout=timeout
                    ),
                    timeout=30.0,
                )
            except asyncio.TimeoutError:
                self.ten_env.log_error("WebSocket connection timeout")
                raise
            except Exception as e:
                self.ten_env.log_error(f"WebSocket connection failed: {e}")
                raise

            self.connected = True
            self.sent_user_audio_duration_ms_before_last_reset += (
                self.audio_timeline.get_total_user_audio_duration()
            )
            self.audio_timeline.reset()
            self._utterance_start_ms = None

            # Start message processing task
            self._message_task = asyncio.create_task(self._process_messages())

            self.ten_env.log_info(
                "start_connection completed",
                category=LOG_CATEGORY_VENDOR,
            )

        except Exception as e:
            self.ten_env.log_error(
                f"KEYPOINT start_connection failed: invalid vendor config: {e}"
            )
            self.connected = False
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=str(e),
                ),
            )
            self._schedule_reconnect()

    def _schedule_reconnect(self) -> None:
        """Trigger a reconnection attempt on an independent task.

        Reconnection must never be awaited from within `_message_task`. The
        reconnect path runs `start_connection()` -> `stop_connection()`, and
        `stop_connection()` cancels `_message_task`. Awaiting the reconnect
        inline would therefore cancel the very task executing it, tearing the
        flow down before `start_connection()` can spawn a fresh message task.

        Running it as a standalone task lets the message loop return cleanly
        while the reconnect proceeds. Tasks are retained in a set (and removed
        on completion) so they are not garbage collected mid-flight and can be
        cancelled on shutdown.
        """
        task = asyncio.create_task(self._handle_reconnect())
        self._reconnect_tasks.add(task)
        task.add_done_callback(self._reconnect_tasks.discard)

    async def _process_messages(self) -> None:
        """Process incoming messages from the WebSocket."""
        assert self.ws is not None

        try:
            async for msg in self.ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    try:
                        data = json.loads(msg.data)
                        await self._handle_message(data)
                    except json.JSONDecodeError as e:
                        self.ten_env.log_warn(
                            f"Invalid JSON received from WebSocket: {e}",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        continue
                    except Exception as e:
                        self.ten_env.log_error(
                            f"Error processing WebSocket message: {e}",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        raise

                elif msg.type == aiohttp.WSMsgType.ERROR:
                    error_msg = f"WebSocket error: {self.ws.exception()}"
                    self.ten_env.log_error(
                        error_msg,
                        category=LOG_CATEGORY_VENDOR,
                    )
                    raise RuntimeError(error_msg)

                elif msg.type in (
                    aiohttp.WSMsgType.CLOSED,
                    aiohttp.WSMsgType.CLOSE,
                    aiohttp.WSMsgType.CLOSING,
                ):
                    self.ten_env.log_info(
                        f"WebSocket closed: {msg.type}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    # WebSocket closed unexpectedly, trigger reconnection
                    if not self.stopped:
                        await self.send_asr_error(
                            ModuleError(
                                module=MODULE_NAME_ASR,
                                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                                message=f"WebSocket closed unexpectedly: {msg.type}",
                            ),
                        )
                        # Schedule (do not await) so this task can exit before
                        # the reconnect path cancels it via stop_connection.
                        self._schedule_reconnect()
                    break

        except Exception as e:
            self.ten_env.log_error(
                f"Error in message processing loop: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            if not self.stopped:
                # Send error before attempting reconnection
                await self.send_asr_error(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message=f"WebSocket error, attempting reconnection: {str(e)}",
                    ),
                )
                # Schedule (do not await): this runs inside `_message_task`,
                # which the reconnect path cancels via stop_connection.
                self._schedule_reconnect()

    async def _handle_message(self, data: dict) -> None:
        """Handle different types of messages from the Pulse streaming API."""
        try:
            msg_type = data.get("type")
            if not msg_type:
                self.ten_env.log_warn(
                    "Received message without type field",
                    category=LOG_CATEGORY_VENDOR,
                )
                return

            if msg_type == "transcription":
                await self._handle_transcription(data)
            elif msg_type == "error":
                await self._handle_error_message(data)
            else:
                self.ten_env.log_debug(
                    f"Unknown message type: {msg_type}",
                    category=LOG_CATEGORY_VENDOR,
                )

        except Exception as e:
            self.ten_env.log_error(
                f"Unexpected error handling message: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise

    async def _handle_transcription(self, data: dict) -> None:
        """Handle transcription result messages."""
        assert self.config is not None

        # Reset the retry budget only once the vendor delivers results —
        # resetting right after the handshake lets an accept-then-close
        # failure reconnect forever without ever going fatal.
        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()

        transcript_text = data.get("transcript", "")
        is_final = bool(data.get("is_final", False))
        language = self.config.report_language(data.get("language"))

        if not transcript_text:
            # Pulse emits an empty final transcript when finalize is
            # requested over silence — the finalize handshake must still
            # complete even though there is no result to forward.
            if is_final:
                self._utterance_start_ms = None
                await self._finalize_end()
            self.ten_env.log_debug(
                "Received empty transcript",
                category=LOG_CATEGORY_VENDOR,
            )
            return

        try:
            # Offset of the current vendor session within the whole
            # user-audio timeline (audio sent before the last reconnect).
            session_offset_ms = (
                self.sent_user_audio_duration_ms_before_last_reset
            )

            words = data.get("words") or []
            if words:
                # Word timings are seconds relative to the session start.
                start_s = words[0].get("start", 0.0)
                end_s = words[-1].get("end", start_s)
                start_ms = session_offset_ms + int(start_s * 1000)
                duration_ms = max(0, int((end_s - start_s) * 1000))
            else:
                total_audio_sent_ms = (
                    self.audio_timeline.get_total_user_audio_duration()
                    + session_offset_ms
                )
                if self._utterance_start_ms is None:
                    # Without word timings the closest anchor to the
                    # utterance start is the stream position when its
                    # first transcript arrives; latching it keeps
                    # interim timestamps monotonic and ahead of the
                    # word-timed final.
                    self._utterance_start_ms = max(0, total_audio_sent_ms)
                start_ms = self._utterance_start_ms
                duration_ms = max(0, total_audio_sent_ms - start_ms)

            asr_result = ASRResult(
                text=transcript_text,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=language,
                words=[],
            )

            await self.send_asr_result(asr_result)
            if is_final:
                self._utterance_start_ms = None
                await self._finalize_end()

            self.ten_env.log_debug(
                f"Transcript processed (final={is_final}): {transcript_text[:50]}...",
                category=LOG_CATEGORY_VENDOR,
            )

        except Exception as e:
            self.ten_env.log_error(
                f"Error processing transcription data: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise

    async def _handle_error_message(self, data: dict) -> None:
        """Handle error messages from the API."""
        error_info = data.get("message") or data.get("error", "Unknown error")
        error_code = data.get("code") or data.get("error_code") or "unknown"

        self.ten_env.log_error(
            f"API error received: {error_info} (code: {error_code})",
            category=LOG_CATEGORY_VENDOR,
        )

        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=str(error_info),
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(error_code),
                message=str(error_info),
            ),
        )

    async def _handle_reconnect(self):
        """
        Handle a single reconnection attempt using the ReconnectManager.
        Connection success is determined by the start_connection callback.
        """
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        # Check if we can still retry
        if not self.reconnect_manager.can_retry():
            self.ten_env.log_warn("No more reconnection attempts allowed")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message="No more reconnection attempts allowed",
                )
            )
            return

        # Attempt a single reconnection
        success = await self.reconnect_manager.handle_reconnect(
            connection_func=self.start_connection,
            error_handler=self.send_asr_error,
        )

        if success:
            self.ten_env.log_debug(
                "Reconnection attempt initiated successfully"
            )
        else:
            info = self.reconnect_manager.get_attempts_info()
            self.ten_env.log_debug(
                f"Reconnection attempt failed. Status: {info}"
            )

    @override
    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_info(
            f"vendor_cmd: finalize start at {self.last_finalize_timestamp}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self._send_finalize()

    async def _finalize_end(self) -> None:
        """Handle finalize end logic."""
        if self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT finalize end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def stop_connection(self) -> None:
        """Stop the Smallest AI connection."""
        try:
            # Cancel message processing task
            if self._message_task and not self._message_task.done():
                self._message_task.cancel()
                try:
                    await self._message_task
                except asyncio.CancelledError:
                    pass

            # Close WebSocket
            if self.ws and not self.ws.closed:
                await self.ws.close()
                self.ws = None

            # Close session
            if self.session and not self.session.closed:
                await self.session.close()
                self.session = None

            self.connected = False
            self.ten_env.log_info("smallest connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping smallest connection: {e}")

    async def _send_finalize(self) -> None:
        """Ask Pulse to finalize pending audio; the session stays open."""
        if not self.is_connected() or self.ws is None:
            # No vendor session to flush (buffered audio is retained
            # client-side and re-sent after reconnect), so complete the
            # finalize handshake locally — otherwise asr_finalize_end is
            # never emitted and the turn stalls.
            self.ten_env.log_warn(
                "smallest finalize requested while disconnected",
                category=LOG_CATEGORY_VENDOR,
            )
            await self._finalize_end()
            return
        try:
            finalize_message = {"type": "finalize"}
            await self.ws.send_str(json.dumps(finalize_message))
            self.ten_env.log_debug("smallest finalize sent")
        except Exception as e:
            self.ten_env.log_error(f"Error sending smallest finalize: {e}")
            await self._finalize_end()
            if not self.stopped:
                self._schedule_reconnect()

    @override
    def is_connected(self) -> bool:
        return self.connected and self.ws is not None and not self.ws.closed

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        assert self.config is not None

        # Guard the send instead of asserting the socket exists: after a failed
        # reconnect `self.ws` can be None, and an assert would raise (or be
        # stripped under -O) instead of failing gracefully. Mirrors the
        # is_connected()/ws check in `_send_finalize` and pipecat's
        # `state is State.OPEN` send guard.
        if not self.is_connected() or self.ws is None:
            self.ten_env.log_error("Smallest AI connection is not established")
            return False

        buf = frame.lock_buf()
        try:
            audio_data = bytes(buf)

            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)

            self.audio_timeline.add_user_audio(
                int(len(buf) / (self.config.sample_rate / 1000 * 2))
            )

            # Pulse accepts raw PCM binary frames directly.
            await self.ws.send_bytes(audio_data)
            return True

        except Exception as e:
            self.ten_env.log_error(f"Error sending audio: {e}")
            if not self.stopped:
                self._schedule_reconnect()
            return False
        finally:
            frame.unlock_buf(buf)
