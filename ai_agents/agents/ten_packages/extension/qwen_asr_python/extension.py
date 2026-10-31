import asyncio
import copy
import os
from typing import Any

from typing_extensions import override

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.struct import ASRWord
from ten_runtime import AsyncTenEnv, AudioFrame

from .client import QwenASRClient, QwenProtocolError
from .config import MODEL_NAME, QwenASRConfig


MODULE_NAME_ASR = ModuleType.ASR
BUFFER_LIMIT = 10 * 1024 * 1024


class QwenASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: QwenASRConfig | None = None
        self.client: QwenASRClient | None = None
        self.audio_dumper: Dumper | None = None
        self.connected = False
        self._init_failed = False
        self._stop_requested = False
        self._fatal_error = False
        self._send_lock = asyncio.Lock()
        self._reconnect_task: asyncio.Task[None] | None = None
        self._reconnect_attempt = 0
        self._generation = 0
        self._connection_start = 0.0
        self._task_audio_start_ms = 0

    @override
    def vendor(self) -> str:
        return "qwen"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        config = self.config
        return {
            "vendor": self.vendor(),
            "model": MODEL_NAME,
            "url": config.url if config else "",
            "sample_rate": config.sample_rate if config else 16000,
            "api_key": config.api_key if config else "",
        }

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, error = await ten_env.get_property_to_json("")
        try:
            if error:
                raise ValueError(str(error))
            self.config = QwenASRConfig.model_validate_json(config_json)
            self.config.apply_defaults()
            self.config.validate_config()
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                dump_path = os.path.join(self.config.dump_path, "qwen_asr.pcm")
                self.audio_dumper = Dumper(dump_path)
                await self.audio_dumper.start()
        except Exception as exc:
            self._init_failed = True
            self.auto_connect = False
            self.config = None
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(exc),
                )
            )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self._stop_requested = True
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()
            try:
                await self._reconnect_task
            except asyncio.CancelledError:
                pass
            self._reconnect_task = None
        await super().on_stop(ten_env)

    @override
    async def start_connection(self) -> None:
        if self._init_failed or self._stop_requested or self._fatal_error:
            return
        assert self.config is not None
        self._connection_start = asyncio.get_event_loop().time()
        generation = self._generation + 1
        self._generation = generation
        await self.stop_connection()
        client = QwenASRClient(
            url=self.config.url,
            api_key=self.config.api_key,
            run_task_factory=self.config.build_run_task,
            finish_task_factory=self.config.build_finish_task,
            event_callback=lambda event: self._handle_event(event, generation),
            close_callback=lambda reason: self._handle_close(
                reason, generation
            ),
            timeout_s=self.config.connect_timeout_s,
        )
        self.client = client
        try:
            await client.connect()
            if self._stop_requested or generation != self._generation:
                await client.close()
                return
            self.connected = True
            self._reconnect_attempt = 0
            self._start_task_timeline()
            await self.on_connected()
            await self.send_connect_delay_metrics(
                int(
                    (asyncio.get_event_loop().time() - self._connection_start)
                    * 1000
                )
            )
        except asyncio.CancelledError:
            await client.close()
            raise
        except Exception as exc:
            if generation != self._generation:
                return
            self.connected = False
            await client.close()
            self.client = None
            fatal = self._is_fatal_exception(exc)
            self._fatal_error = fatal
            await self._send_vendor_error(exc, fatal=fatal)
            await self.on_disconnected(
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=str(exc),
            )
            if not fatal and not self._stop_requested:
                self._schedule_reconnect()

    @override
    async def stop_connection(self) -> None:
        client = self.client
        self.client = None
        self.connected = False
        if client:
            await client.close()

    @override
    def is_connected(self) -> bool:
        return (
            self.connected and self.client is not None and self.client.connected
        )

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=BUFFER_LIMIT)

    @override
    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 16000

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        if not self.is_connected() or self.client is None:
            return False
        buf = None
        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)
            async with self._send_lock:
                client = self.client
                if not self.is_connected() or client is None:
                    return False
                if not client.task_ready:
                    await client.start_task()
                    self._start_task_timeline()
                await client.send_audio(audio_data)
                self.audio_timeline.add_user_audio(
                    int(
                        len(audio_data)
                        / (self.input_audio_sample_rate() / 1000 * 2)
                    )
                )
                self.session_id = session_id
                if self.audio_dumper:
                    await self.audio_dumper.push_bytes(audio_data)
            return True
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            if not self.stopped:
                await self._send_vendor_error(exc, fatal=False)
            return False
        finally:
            if buf is not None:
                frame.unlock_buf(buf)

    @override
    async def finalize(self, _session_id: str | None) -> None:
        client = self.client
        try:
            async with self._send_lock:
                client = self.client
                if self.is_connected() and client is not None:
                    try:
                        await asyncio.wait_for(
                            client.finish_task(),
                            timeout=(
                                (self.config.finalize_timeout_ms / 1000)
                                if self.config
                                else 5
                            ),
                        )
                    except asyncio.CancelledError:
                        raise
                    except Exception as exc:
                        await self._send_vendor_error(exc, fatal=False)
        finally:
            if self.last_finalize_time is not None:
                ttlw = int(
                    (asyncio.get_event_loop().time() - self.last_finalize_time)
                    * 1000
                )
                await self._send_metrics_ttlw(ttlw)
                self.last_finalize_time = None
            await self.send_asr_finalize_end()

    async def _handle_event(
        self, event: dict[str, Any], generation: int
    ) -> None:
        if generation != self._generation or self._stop_requested:
            return
        event_name = (event.get("header") or {}).get("event")
        if event_name == "result-generated":
            await self._handle_result(event)

    async def _handle_result(self, event: dict[str, Any]) -> None:
        payload = event.get("payload") or {}
        if payload.get("heartbeat"):
            return
        output = payload.get("output") or {}
        sentence = output.get("sentence") or {}
        text = str(sentence.get("text") or payload.get("text") or "").strip()
        if not text:
            return
        final = bool(sentence.get("sentence_end", False))
        begin_time = int(sentence.get("begin_time") or 0)
        end_time = sentence.get("end_time")
        start_ms = self._timeline_time(begin_time)
        duration = max(
            0, self._timeline_time(int(end_time or begin_time)) - start_ms
        )
        words: list[ASRWord] = []
        for word in sentence.get("words") or []:
            word_start = int(word.get("begin_time") or 0)
            word_end = int(word.get("end_time") or word_start)
            word_start_ms = self._timeline_time(word_start)
            words.append(
                ASRWord(
                    word=str(word.get("text") or "")
                    + str(word.get("punctuation") or ""),
                    start_ms=word_start_ms,
                    duration_ms=max(
                        0, self._timeline_time(word_end) - word_start_ms
                    ),
                    stable=final or bool(word.get("fixed", False)),
                )
            )
        metadata = copy.deepcopy(self.metadata) if self.metadata else {}
        metadata["asr_info"] = {
            "vendor": self.vendor(),
            "model": MODEL_NAME,
            "request_id": payload.get("request_id"),
            "sentence_id": sentence.get("sentence_id"),
            "locked": False,
            "usage": payload.get("usage") or {},
        }
        await self.send_asr_result(
            ASRResult(
                text=text,
                final=final,
                start_ms=start_ms,
                duration_ms=duration,
                language=self.config.language if self.config else "und",
                words=words,
                metadata=metadata,
            )
        )
        self.ten_env.log_debug(
            f"qwen result: final={final} text={text!r}",
            category=LOG_CATEGORY_VENDOR,
        )

    def _timeline_time(self, vendor_time_ms: int) -> int:
        return self.audio_timeline.get_audio_duration_before_time(
            self._task_audio_start_ms + vendor_time_ms
        )

    def _start_task_timeline(self) -> None:
        self._task_audio_start_ms = (
            self.audio_timeline.total_user_audio_duration
            + self.audio_timeline.total_silence_audio_duration
        )

    async def _handle_close(self, reason: str, generation: int) -> None:
        if generation != self._generation or self._stop_requested:
            return
        client = self.client
        self.connected = False
        self.client = None
        if client is not None:
            await client.close()
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(), code="websocket_closed", message=reason
        )
        await self.on_disconnected(
            code=ModuleErrorCode.NON_FATAL_ERROR.value,
            message=reason,
            vendor_info=vendor_info,
        )
        await self._send_vendor_error(
            QwenProtocolError(reason, "websocket_closed"), fatal=False
        )
        self._schedule_reconnect()

    async def _send_vendor_error(self, exc: Exception, fatal: bool) -> None:
        if self._stop_requested:
            return
        message = str(exc)
        vendor_code = str(getattr(exc, "code", "unknown"))
        self.ten_env.log_error(
            f"qwen vendor error: code={vendor_code} message={message}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=message,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(), code=vendor_code, message=message
            ),
        )

    @staticmethod
    def _is_fatal_exception(exc: Exception) -> bool:
        status = getattr(exc, "status_code", None)
        response = getattr(exc, "response", None)
        if status is None and response is not None:
            status = getattr(response, "status_code", None)
        return status in {401, 403}

    def _schedule_reconnect(self) -> None:
        if self._stop_requested or self._fatal_error:
            return
        if self._reconnect_task and not self._reconnect_task.done():
            return
        self._reconnect_task = asyncio.create_task(self._reconnect_loop())

    async def _reconnect_loop(self) -> None:
        assert self.config is not None
        try:
            for attempt in range(1, self.config.reconnect_max_attempts + 1):
                if self._stop_requested or self._fatal_error:
                    return
                self._reconnect_attempt = attempt
                await asyncio.sleep(min(2 ** (attempt - 1), 8))
                await self.start_connection()
                if self.is_connected():
                    return
        except asyncio.CancelledError:
            raise
        finally:
            if self._reconnect_task is asyncio.current_task():
                self._reconnect_task = None
