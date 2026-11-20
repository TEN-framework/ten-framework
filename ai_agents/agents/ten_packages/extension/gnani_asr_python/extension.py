import asyncio
import json
import os
import time
from collections import deque
from dataclasses import dataclass

from pydantic import ValidationError
from websockets.asyncio.client import connect
from websockets.exceptions import WebSocketException

from ten_ai_base.dumper import Dumper
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
from ten_ai_base.struct import ASRResult
from ten_runtime import Data

from .config import PrismaConfig


FRAME_BYTES = 1024
QUEUE_BYTES = 16000 * 2 * 5


@dataclass
class DroppedAudio:
    size: int


class OrderedInputs(deque):
    """Queue used by the ASR base to serialize audio alongside finalize data."""

    async def put(self, frame):
        self.append(frame)


class GnaniASRExtension(AsyncASRBaseExtension):
    url = "wss://api.vachana.ai/stt/v3/stream"

    def __init__(self, name):
        super().__init__(name)
        self.dumper = None
        self.config = None
        self.ws = None
        self.receiver = None
        self.consumer = None
        self.retry = None
        self.ready = asyncio.Event()
        self.changed = asyncio.Event()
        self.inputs = OrderedInputs()
        self.audio_frames_queue = self.inputs
        self.generation = 0
        self.connector = None
        self.finalize_times = deque()
        self.last_event = 0.0
        self.turn_results = 0
        self.processing = 0
        self.available = asyncio.Event()
        self.queued_bytes = 0
        self.tail = bytearray()
        self.starts = {}
        self.unidentified_starts = deque()
        self.completed = set()
        self.finalize_metadata = None
        self.fatal = False
        self.next_send = 0.0
        self.origin = 0
        self.last_end = 0
        self.user_bytes = 0
        self.dirty = False

    def vendor(self):
        return "gnani"

    def vendor_metadata(self):
        return {"model": self.config.model} if self.config else {}

    def input_audio_sample_rate(self):
        return 16000

    def is_connected(self):
        return self.ws is not None

    async def error(self, message, fatal=False, vendor_code="prisma"):
        self.fatal |= fatal
        code = ModuleErrorCode.FATAL_ERROR if fatal else ModuleErrorCode.NON_FATAL_ERROR
        await self.send_asr_error(
            ModuleError(module="asr", code=code.value, message=message),
            ModuleErrorVendorInfo(vendor="gnani", code=vendor_code, message=message),
        )

    async def on_init(self, ten_env):
        await super().on_init(ten_env)
        raw, err = await ten_env.get_property_to_json("params")
        try:
            if err:
                raise ValueError("missing Prisma params")
            self.config = PrismaConfig(**json.loads(raw))
            ten_env.log_info(
                f"config: {self.config.model_dump_json(exclude={'api_key'})}"
            )
            dump, _ = await ten_env.get_property_bool("dump")
            if dump:
                path, _ = await ten_env.get_property_string("dump_path")
                self.dumper = Dumper(os.path.join(path or ".", "gnani_asr_in.pcm"))
                await self.dumper.start()
        except ValidationError as exc:
            details = ", ".join(err["msg"] for err in exc.errors(include_input=False))
            await self.error(f"invalid Prisma params: {details}", True)
        except TypeError:
            await self.error("invalid Prisma params", True)
        except ValueError as exc:
            await self.error(str(exc), True)

    async def start_connection(self):
        if self.fatal or self.stopped or self.ws or not self.config:
            return
        generation = self.generation
        task = asyncio.create_task(self._connect(generation))
        self.connector = task
        try:
            await task
        except asyncio.CancelledError:
            if self.generation == generation:
                raise
        finally:
            if self.connector is task:
                self.connector = None

    async def _connect(self, generation):
        ws = None
        try:
            started = time.monotonic()
            ws = await connect(
                self.url,
                additional_headers=self.config.headers(),
                open_timeout=5,
                close_timeout=1,
            )
            hello = json.loads(await asyncio.wait_for(ws.recv(), 5))
            if self.stopped or generation != self.generation:
                await ws.close()
                return
            if hello.get("type") == "error" and any(
                term in str(hello.get("message", "")).lower()
                for term in ("concurrent", "capacity", "rate limit")
            ):
                raise OSError("Prisma capacity limit")
            if hello.get("type") != "connected":
                raise ValueError("Prisma rejected stream configuration")
            self.origin = (
                self.audio_timeline.get_total_user_audio_duration() + self.origin
            )
            self.audio_timeline.reset()
            self.last_reported_audio_duration = 0
            self.starts.clear()
            self.unidentified_starts.clear()
            self.completed.clear()
            self.ws = ws
            self.ready.set()
            self.receiver = asyncio.create_task(self.receive(ws))
            await self.on_connected()
            await self.send_connect_delay_metrics(
                int((time.monotonic() - started) * 1000)
            )
        except asyncio.CancelledError:
            if ws:
                await ws.close()
            raise
        except (WebSocketException, OSError, ValueError, asyncio.TimeoutError) as exc:
            if ws:
                await ws.close()
            status = getattr(getattr(exc, "response", None), "status_code", None)
            fatal = status in (400, 401, 402, 403, 404) or isinstance(exc, ValueError)
            await self.error(
                f"Prisma connection failed ({status or type(exc).__name__})",
                fatal,
                str(status or type(exc).__name__),
            )
            await self.on_disconnected(
                code=-1000 if fatal else 1000, message="connection failed"
            )
            self.schedule_retry()

    def schedule_retry(self):
        if (
            not self.stopped
            and not self.fatal
            and (not self.retry or self.retry.done())
        ):
            self.retry = asyncio.create_task(self.reconnect())

    async def reconnect(self):
        delay = 0.25
        while not self.stopped and not self.fatal:
            await asyncio.sleep(delay)
            await self._ensure_connection()
            if self.ws or self.fatal or self.stopped:
                return
            delay = min(delay * 2, 4)

    async def receive(self, ws):
        try:
            async for message in ws:
                if self.ws is not ws:
                    break
                event = json.loads(message)
                if not isinstance(event, dict):
                    raise ValueError("invalid Prisma message")
                kind = event.get("type")
                uid = event.get("utterance_id") or event.get("segment_id")
                if kind == "transcript" and not uid:
                    raise ValueError("missing utterance identifier")
                self.last_event = time.monotonic()
                if kind == "processing":
                    self.processing += 1
                elif kind == "speech_start":
                    offset = event.get("t_ms", self.sent_ms())
                    if uid:
                        self.starts[uid] = offset
                    else:
                        self.unidentified_starts.append(offset)
                elif kind == "transcript" and uid not in self.completed:
                    upper = self.sent_ms()
                    offset = self.starts.pop(uid, None)
                    if offset is None and self.unidentified_starts:
                        offset = self.unidentified_starts.popleft()
                    if offset is None:
                        offset = self.last_end - self.origin
                    start = min(upper, max(0, offset))
                    end = min(upper, start + max(0, int(event["audio_duration_ms"])))
                    begin = (
                        self.origin
                        + self.audio_timeline.get_audio_duration_before_time(start)
                    )
                    finish = (
                        self.origin
                        + self.audio_timeline.get_audio_duration_before_time(end)
                    )
                    self.last_end = finish
                    if event.get("text"):
                        await self.send_asr_result(
                            ASRResult(
                                text=event["text"],
                                final=True,
                                start_ms=begin,
                                duration_ms=finish - begin,
                                words=[],
                                language=event.get("detected_language_code")
                                or self.config.language,
                            )
                        )
                    self.completed.add(uid)
                    self.turn_results += 1
                    self.processing = max(0, self.processing - 1)
                elif kind == "error":
                    reason = str(event.get("message") or "unspecified error")
                    reason = reason.replace(self.config.api_key, "[redacted]")[:240]
                    await self.error(
                        f"Prisma stream error: {reason}",
                        vendor_code=str(event.get("code") or "error"),
                    )
                    self.processing = max(0, self.processing - 1)
                    self.starts.pop(uid, None)
                self.changed.set()
        except (WebSocketException, OSError, ValueError, TypeError, KeyError):
            await self.error("Prisma stream interrupted")
        finally:
            if self.ws is ws:
                self.ws = None
                self.ready.clear()
                self.changed.set()
                await ws.close()
                await self.on_disconnected(code=1000, message="stream closed")
                self.schedule_retry()

    def sent_ms(self):
        return (
            self.audio_timeline.total_user_audio_duration
            + self.audio_timeline.total_silence_audio_duration
        )

    async def on_audio_frame(self, ten_env, audio_frame):
        if self.fatal or self.stopped:
            return
        size = len(audio_frame.get_buf())
        if (
            size % 2
            or audio_frame.get_sample_rate() not in (0, 16000)
            or audio_frame.get_number_of_channels() not in (0, 1)
            or audio_frame.get_bytes_per_sample() not in (0, 2)
        ):
            await self.error("expected 16 kHz mono PCM16")
            return
        if not size:
            return
        if self.dumper:
            await self.dumper.push_bytes(bytes(audio_frame.get_buf()))
        await super().on_audio_frame(ten_env, audio_frame)
        self.queued_bytes += size
        dropped = 0
        while self.queued_bytes + len(self.tail) > QUEUE_BYTES and self.inputs:
            for i, item in enumerate(self.inputs):
                if not isinstance(item, (Data, DroppedAudio)):
                    n = len(item.get_buf())
                    if i and isinstance(self.inputs[i - 1], DroppedAudio):
                        self.inputs[i - 1].size += n
                        del self.inputs[i]
                    else:
                        self.inputs[i] = DroppedAudio(n)
                    self.queued_bytes -= n
                    dropped += n
                    break
            else:
                break
        self.available.set()
        if dropped:
            await self.error("Prisma input buffer overflow")

    async def on_data(self, ten_env, data):
        if data.get_name() == "asr_finalize":
            self.finalize_times.append(time.monotonic())
            self.inputs.append(data)
            self.available.set()
        else:
            await super().on_data(ten_env, data)

    async def _audio_frame_consumer(self):
        self.consumer = asyncio.current_task()
        while not self.stopped:
            await self.available.wait()
            if not self.inputs:
                self.available.clear()
                continue
            # Keep unsent frames in the bounded FIFO until the socket is ready.
            if (
                not self.ready.is_set()
                and not isinstance(self.inputs[0], (Data, DroppedAudio))
                and not self.fatal
            ):
                try:
                    await asyncio.wait_for(self.ready.wait(), 1)
                except asyncio.TimeoutError:
                    if (
                        not self.finalize_times
                        or time.monotonic() - self.finalize_times[0] < 5
                    ):
                        continue
                    dropped = 0
                    while self.inputs and not isinstance(self.inputs[0], Data):
                        pending = self.inputs.popleft()
                        if isinstance(pending, DroppedAudio):
                            dropped += pending.size
                        else:
                            size = len(pending.get_buf())
                            dropped += size
                            self.queued_bytes -= size
                    self.audio_timeline.add_dropped_audio(
                        (dropped + len(self.tail)) // 32
                    )
                    self.tail.clear()
                    await self.error("Prisma unavailable at finalize deadline")
                    continue
            item = self.inputs.popleft()
            try:
                if isinstance(item, DroppedAudio):
                    if self.tail and self.is_connected():
                        await self.packet(min(FRAME_BYTES, len(self.tail)))
                    self.audio_timeline.add_dropped_audio(item.size // 32)
                elif isinstance(item, Data):
                    self.finalize_times.popleft()
                    raw, _ = item.get_property_to_json("metadata")
                    self.finalize_metadata = json.loads(raw) if raw else None
                    await super().on_data(self.ten_env, item)
                else:
                    self.queued_bytes -= len(item.get_buf())
                    if not self.fatal:
                        await self.send_audio(item, None)
            except (ValueError, TypeError):
                await self.error("invalid TEN input metadata")
            except (OSError, WebSocketException, asyncio.TimeoutError):
                await self.error("Prisma audio delivery interrupted")

    async def packet(self, size):
        await asyncio.wait_for(self.ready.wait(), 15)
        ws = self.ws
        await asyncio.sleep(max(0, self.next_send - time.monotonic()))
        if not ws or ws is not self.ws:
            raise OSError("socket closed")
        packet = bytes(self.tail[:size]).ljust(FRAME_BYTES, b"\0")
        del self.tail[:size]
        self.next_send = time.monotonic() + 0.032
        try:
            await asyncio.wait_for(ws.send(packet), 5)
        except (WebSocketException, OSError, asyncio.TimeoutError):
            # Delivery is ambiguous once a write begins. Never replay it.
            self.audio_timeline.add_dropped_audio(size // 32)
            await ws.close()
            raise
        before_ms = self.user_bytes // 32
        self.user_bytes += size
        real_ms = self.user_bytes // 32 - before_ms
        self.audio_timeline.add_user_audio(real_ms)
        self.audio_timeline.add_silence_audio(32 - real_ms)
        if self.first_audio_time is None:
            self.first_audio_time = time.monotonic()
        self.sent_buffer_length += size
        self.dirty = True

    async def send_audio(self, frame, session_id):
        raw, _ = frame.get_property_to_json("metadata")
        try:
            metadata = json.loads(raw) if raw else {}
            if not isinstance(metadata, dict):
                raise ValueError
        except ValueError:
            await self.error("invalid TEN audio metadata")
            return False
        new_session = metadata.get("session_id", self.session_id)
        if self.session_id is not None and new_session != self.session_id:
            await self.stop_connection()
            self.tail.clear()
            self.audio_timeline.reset()
            self.origin = self.last_end = self.user_bytes = 0
            self.dirty = False
            self.schedule_retry()
        self.session_id = new_session
        self.metadata = metadata
        self.tail.extend(frame.get_buf())
        while len(self.tail) >= FRAME_BYTES:
            await self.packet(FRAME_BYTES)
        return True

    async def finalize(self, session_id):
        metadata = (
            self.finalize_metadata
            if isinstance(self.finalize_metadata, dict)
            else self.metadata
        )
        try:
            while self.tail:
                await self.packet(min(FRAME_BYTES, len(self.tail)))
            if self.dirty:
                ws = self.ws
                if not ws:
                    raise OSError("disconnected")
                await asyncio.wait_for(ws.send(json.dumps({"type": "commit"})), 5)
                # Commit has no drain acknowledgement. Hold this turn's context
                # for delayed events, then retire its socket before new audio.
                self.last_event = time.monotonic()
                deadline = self.last_event + 5
                while True:
                    if self.ws is not ws:
                        raise OSError("disconnected")
                    now = time.monotonic()
                    pending = self.starts or self.unidentified_starts or self.processing
                    if (
                        not pending
                        and self.turn_results
                        and now - self.last_event >= 0.5
                    ):
                        break
                    if now >= deadline:
                        if pending:
                            raise asyncio.TimeoutError
                        break
                    self.changed.clear()
                    try:
                        await asyncio.wait_for(
                            self.changed.wait(), min(0.5, deadline - now)
                        )
                    except asyncio.TimeoutError:
                        pass
                await self.stop_connection()
                self.schedule_retry()
        except (asyncio.TimeoutError, OSError, WebSocketException):
            self.audio_timeline.add_dropped_audio(len(self.tail) // 32)
            self.tail.clear()
            await self.error("Prisma finalize interrupted")
            await self.stop_connection()
            self.schedule_retry()
        self.dirty = False
        self.turn_results = 0
        self.processing = 0
        last_metadata = self.metadata
        try:
            self.metadata = metadata
            await self.send_asr_finalize_end()
        finally:
            self.metadata = last_metadata

    async def stop_connection(self):
        self.generation += 1
        connector = self.connector
        if connector and connector is not asyncio.current_task():
            connector.cancel()
            await asyncio.gather(connector, return_exceptions=True)
        ws, self.ws = self.ws, None
        self.ready.clear()
        if self.receiver:
            self.receiver.cancel()
            await asyncio.gather(self.receiver, return_exceptions=True)
        if ws:
            await ws.close()
            await self.on_disconnected(code=1000, message="stream closed")

    async def on_stop(self, ten_env):
        self.stopped = True
        tasks = [t for t in (self.retry, self.consumer) if t]
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        await super().on_stop(ten_env)
        if self.dumper:
            await self.dumper.stop()
