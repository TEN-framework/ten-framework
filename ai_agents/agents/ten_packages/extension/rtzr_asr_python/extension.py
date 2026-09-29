import asyncio
import json
import os
import time
from collections import deque
from typing import Any

import aiohttp
from pydantic import ValidationError
from ten_ai_base.asr import (
    ASRBufferConfigModeKeep,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from ten_ai_base.struct import ASRResult, ASRWord
from ten_runtime import AsyncTenEnv, AudioFrame, Data

from .client import RTZRClient
from .config import RTZRASRConfig


class RTZRASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: RTZRASRConfig | None = None
        self.client: RTZRClient | None = None
        self.ws: aiohttp.ClientWebSocketResponse | None = None
        self._receiver: asyncio.Task | None = None
        self._retry: asyncio.Task | None = None
        self._consumer: asyncio.Task | None = None
        self._connect_lock = asyncio.Lock()
        self._final = asyncio.Event()
        self._fatal = False
        self._attempts = 0
        self._sent_bytes = 0
        self._connection_offset_ms = 0
        self._final_end_ms = 0
        self._pending_result = False
        self._received_result = False
        self._dumper: Dumper | None = None
        self._pending = deque()
        self._pending_bytes = 0
        self._send_lock = asyncio.Lock()
        # Backpressure also bounds frames waiting for a slow send/finalize.
        self.audio_frames_queue = asyncio.Queue(maxsize=1000)

    def vendor(self) -> str:
        return "rtzr"

    def vendor_metadata(self) -> dict[str, Any]:
        if not self.config:
            return {}
        return {
            "model": self.config.params["model_name"],
            "url": self.config.params["websocket_url"],
        }

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        try:
            raw, error = await ten_env.get_property_to_json("")
            if error:
                raise ValueError("cannot read RTZR properties")
            self.config = RTZRASRConfig.model_validate_json(raw)
            self.client = RTZRClient(self.config)
            ten_env.log_info(
                f"config: {self.config.to_str()}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                self._dumper = Dumper(
                    os.path.join(self.config.dump_path, "rtzr_asr_in.pcm")
                )
                await self._dumper.start()
        except ValidationError as exc:
            # Pydantic's default string includes the complete input config.
            message = "invalid RTZR configuration"
            message += ": " + "; ".join(
                item["msg"]
                for item in exc.errors(
                    include_input=False, include_context=False
                )
            )
            await self._error(message, fatal=True)
        except (ValueError, OSError):
            await self._error(
                "cannot initialize RTZR configuration/dump", fatal=True
            )

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() == "asr_finalize":
            # The base uses separate callbacks for audio and data. Serialize
            # them here so Finalize cannot overtake queued audio or another turn.
            await self.audio_frames_queue.put(data)
        else:
            await super().on_data(ten_env, data)

    async def _audio_frame_consumer(self) -> None:
        self._consumer = asyncio.current_task()
        while not self.stopped:
            item = await self.audio_frames_queue.get()
            try:
                if isinstance(item, Data):
                    await super().on_data(self.ten_env, item)
                elif not self._fatal:
                    await self._handle_audio_frame(self.ten_env, item)
            except Exception as exc:
                await self._error(f"RTZR input failed ({type(exc).__name__})")
            finally:
                self.audio_frames_queue.task_done()

    async def start_connection(self) -> None:
        async with self._connect_lock:
            if self.stopped or self._fatal or self.is_connected():
                return
            if not self.client:
                return
            try:
                start = time.monotonic()
                self.ws = await self.client.connect()
                self._connection_offset_ms = self._duration_ms()
                self._pending_result = False
                self._received_result = False
                self._receiver = asyncio.create_task(self._receive(self.ws))
                await self.on_connected()
                await self.send_connect_delay_metrics(
                    max(1, int((time.monotonic() - start) * 1000))
                )
                await self._flush_frames()
            except Exception as exc:
                status = getattr(exc, "status", None)
                fatal = status in (400, 401, 403, 404) or isinstance(
                    exc, ValueError
                )
                await self._error(
                    "RTZR connection failed",
                    fatal=fatal,
                    code=str(status or type(exc).__name__),
                )
                await self.on_disconnected(
                    code=-1000 if fatal else 1000,
                    message="RTZR connection failed",
                )
                self._schedule_retry()

    def _schedule_retry(self) -> None:
        if self.stopped or self._fatal:
            return
        if self._retry is None or self._retry.done():
            self._retry = asyncio.create_task(self._reconnect())
            self._retry.add_done_callback(self._retry_done)

    def _retry_done(self, task) -> None:
        if not task.cancelled():
            task.exception()
        if self._retry is task:
            self._retry = None
            if not self.is_connected():
                self._schedule_retry()

    async def _reconnect(self) -> None:
        while not self.stopped and not self._fatal:
            if self._attempts >= 5:
                await self._error(
                    "RTZR reconnect attempts exhausted", fatal=True
                )
                return
            await asyncio.sleep(0.3 * 2**self._attempts)
            self._attempts += 1
            await self.start_connection()
            if self.is_connected():
                return

    async def _disconnected(self, ws, code: str) -> None:
        if self.ws is not ws or self.stopped:
            return
        self.ws = None
        await self._error("RTZR stream disconnected", code=code)
        await self.on_disconnected(
            code=1000, message="RTZR stream disconnected"
        )
        self._final.set()
        await ws.close()
        self._schedule_retry()

    async def _receive(self, ws) -> None:
        try:
            async for message in ws:
                if message.type == aiohttp.WSMsgType.TEXT:
                    try:
                        await self._result(json.loads(message.data))
                    except (ValueError, TypeError, KeyError, IndexError):
                        await self._error(
                            "invalid RTZR recognition response", code="payload"
                        )
                elif message.type == aiohttp.WSMsgType.ERROR:
                    break
        except (aiohttp.ClientError, OSError):
            pass
        finally:
            await self._disconnected(ws, str(ws.close_code or "websocket"))

    async def _result(self, payload: dict) -> None:
        assert self.config is not None
        # Validate the external response before constructing TEN messages.
        if (
            not isinstance(payload, dict)
            or type(payload.get("final")) is not bool
        ):
            raise ValueError("invalid final flag")
        start, duration = payload["start_at"], payload["duration"]
        if (
            type(start) is not int
            or type(duration) is not int
            or min(start, duration) < 0
        ):
            raise ValueError("invalid recognition timestamps")
        alternatives = payload["alternatives"]
        if not isinstance(alternatives, list):
            raise ValueError("invalid alternatives")
        alternative = alternatives[0] if alternatives else {"text": ""}
        if not isinstance(alternative, dict):
            raise ValueError("invalid alternative")
        text = alternative["text"]
        if not isinstance(text, str):
            raise ValueError("invalid transcript")
        raw_words = alternative.get("words", [])
        if not isinstance(raw_words, list):
            raise ValueError("invalid words")
        for word in raw_words:
            if (
                not isinstance(word, dict)
                or not isinstance(word.get("text"), str)
                or any(
                    type(word.get(key)) is not int or word[key] < 0
                    for key in ("start_at", "duration")
                )
            ):
                raise ValueError("invalid word timing")
        self._attempts = 0
        self._pending_result = not payload["final"]
        self._received_result = True
        if text:
            offset = self._connection_offset_ms + start
            words = [
                ASRWord(
                    word=word["text"],
                    start_ms=offset + word["start_at"],
                    duration_ms=word["duration"],
                    stable=payload["final"],
                )
                for word in raw_words
            ]
            await self.send_asr_result(
                ASRResult(
                    text=text,
                    final=payload["final"],
                    start_ms=offset,
                    duration_ms=duration,
                    language=self.config.language,
                    words=words,
                )
            )
        if payload["final"]:
            self._final_end_ms = self._connection_offset_ms + start + duration
            self._final.set()

    def _duration_ms(self) -> int:
        return self._sent_bytes * 1000 // (2 * self.input_audio_sample_rate())

    async def _handle_audio_frame(self, ten_env, audio_frame) -> None:
        del ten_env
        size = len(audio_frame.get_buf())
        if not size:
            return
        limit = self.buffer_strategy().byte_limit
        if size > limit or size % 2:
            await self._error("invalid PCM16 frame size")
            return
        raw, _ = audio_frame.get_property_to_json("metadata")
        try:
            if raw and not isinstance(json.loads(raw), dict):
                raise ValueError("metadata must be an object")
        except ValueError:
            await self._error("invalid audio metadata")
            return
        async with self._send_lock:
            dropped = 0
            while (
                self._pending and self._pending_bytes - dropped + size > limit
            ):
                old = self._pending.popleft()
                dropped += len(old.get_buf())
            self._pending_bytes -= dropped
            if dropped:
                await self._error(
                    "RTZR audio buffer overflow", code="buffer_overflow"
                )
            self._pending.append(audio_frame)
            self._pending_bytes += size
        await self._flush_frames()

    async def _flush_frames(self) -> None:
        # Keep the head until its send succeeds; the base's generic flush
        # discards failed buffered sends and waits for new audio to resume.
        async with self._send_lock:
            while self._pending and self.is_connected():
                frame = self._pending[0]
                raw, _ = frame.get_property_to_json("metadata")
                if raw:
                    metadata = json.loads(raw)
                    if not isinstance(metadata, dict):
                        raise ValueError("audio metadata must be an object")
                    self.metadata = metadata
                    self.session_id = metadata.get(
                        "session_id", self.session_id
                    )
                if not await self.send_audio(frame, self.session_id):
                    return
                self._pending.popleft()
                size = len(frame.get_buf())
                self._pending_bytes -= size
                self.sent_buffer_length += size
                if self.first_audio_time is None:
                    self.first_audio_time = time.monotonic()

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        del session_id
        ws = self.ws
        if not self.is_connected() or ws is None:
            return False
        audio = bytes(frame.get_buf())
        if len(audio) % 2:
            await self._error("PCM16 frame must contain complete samples")
            return False
        try:
            await ws.send_bytes(audio)
        except (aiohttp.ClientError, OSError) as exc:
            await self._disconnected(ws, type(exc).__name__)
            return False
        before = self._duration_ms()
        self._sent_bytes += len(audio)
        self.audio_timeline.add_user_audio(self._duration_ms() - before)
        if self._dumper:
            await self._dumper.push_bytes(audio)
        return True

    async def finalize(self, session_id: str | None) -> None:
        del session_id
        assert self.config is not None
        ws = self.ws
        if not self.is_connected() or ws is None:
            await self._error("cannot finalize a disconnected RTZR stream")
            return
        self._final.clear()

        async def wait_for_final():
            # A final for earlier audio is not an acknowledgement of all
            # sent audio. Keep waiting while a newer hypothesis is pending.
            while (
                self._pending_result or self._final_end_ms < self._duration_ms()
            ):
                await self._final.wait()
                self._final.clear()
                if self.ws is not ws or not self.is_connected():
                    return

        try:
            await ws.send_json({"type": "Finalize"})
            if self._sent_bytes:
                await asyncio.wait_for(
                    wait_for_final(), self.config.finalize_timeout
                )
            if self.ws is ws and self.is_connected():
                await self.send_asr_finalize_end()
        except asyncio.TimeoutError:
            if (
                self._received_result
                and not self._pending_result
                and self.ws is ws
                and self.is_connected()
            ):
                # RTZR sends no separate Finalize acknowledgement for silence
                # following an automatic final. Report local completion, not a
                # fabricated vendor result. A pending hypothesis still fails.
                await self.send_vendor_metrics({"finalize_without_result": 1})
                await self.send_asr_finalize_end()
            else:
                await self._error(
                    "RTZR finalize timed out", code="finalize_timeout"
                )
        except (aiohttp.ClientError, OSError) as exc:
            await self._disconnected(ws, type(exc).__name__)

    def is_connected(self) -> bool:
        return self.ws is not None and not self.ws.closed

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 16000

    def buffer_strategy(self) -> ASRBufferConfigModeKeep:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    async def _error(
        self, message: str, *, fatal: bool = False, code: str = "client"
    ) -> None:
        self._fatal |= fatal
        await self.send_asr_error(
            ModuleError(
                module="asr",
                code=(
                    ModuleErrorCode.FATAL_ERROR
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR
                ).value,
                message=message,
            ),
            ModuleErrorVendorInfo(vendor="rtzr", code=code, message=message),
        )

    async def stop_connection(self) -> None:
        ws, self.ws = self.ws, None
        if ws is not None:
            try:
                if not ws.closed:
                    await ws.send_str("EOS")
                    if self._receiver:
                        await asyncio.wait_for(
                            asyncio.shield(self._receiver), 2
                        )
            except (aiohttp.ClientError, OSError, asyncio.TimeoutError):
                pass
            finally:
                await ws.close()
        await self._cancel(self._receiver)
        self._receiver = None

    @staticmethod
    async def _cancel(task) -> None:
        if task is not None and task is not asyncio.current_task():
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self.stopped = True
        await self._cancel(self._retry)
        await self._cancel(self._consumer)
        await super().on_stop(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        self.stopped = True
        await self._cancel(self._retry)
        await self._cancel(self._consumer)
        await self.stop_connection()
        if self.client:
            await self.client.close()
        if self._dumper:
            await self._dumper.stop()
        await super().on_deinit(ten_env)
