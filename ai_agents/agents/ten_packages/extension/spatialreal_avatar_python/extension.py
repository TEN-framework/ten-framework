#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""TEN glue for the SpatialReal avatar.

Everything TEN-specific lives here: the property dataclass, the hook
dispatch (audio frames, ``tts_audio_end``/``finalize`` data, the ``flush``
command) and ``ModuleError`` emission (design D8). The work is delegated to
the pure modules: ``config`` validates the properties, ``audio`` resamples
to the session rate, ``pacer`` owns request boundaries and padding, and
``session_client`` keeps the SpatialReal session up.

Nothing raises out of a hook (an uncaught exception ends the TEN process):
a configuration failure is reported as a ``-1012`` error and the extension
stays idle; a session failure is reported by the client and the audio path
simply has nowhere to send until it reconnects.
"""

import asyncio
import json
import traceback
from collections import deque
from dataclasses import dataclass, field
from typing import Any, Optional

from ten_runtime import (  # pylint: disable=import-error
    AsyncExtension,
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
)
from ten_ai_base.config import BaseConfig  # pylint: disable=import-error
from ten_ai_base.message import (  # pylint: disable=import-error
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.utils import encrypt  # pylint: disable=import-error

from .audio import Resampler, duration_ms
from .config import ResolvedConfig, resolve_config
from .pacer import Pacer
from .session_client import (
    CODE_CONFIG,
    CODE_FATAL,
    CODE_NON_FATAL,
    ErrorReport,
    SessionClient,
    SessionSettings,
)

VENDOR = "spatialreal"
# D4: audio arriving before the session is up is queued, bounded to 10 s.
INPUT_QUEUE_MAX_MS = 10_000
# Property keys whose values are secrets; redacted wherever config is logged.
SECRET_KEYS = frozenset({"spatialreal_api_key", "agora_token", "agora_appcert"})


@dataclass
class SpatialRealAvatarConfig(BaseConfig):
    """The two top-level properties (D3): the server-owned channel and the
    operator's ``params`` object. Every other key lives inside ``params``."""

    channel: str = ""
    params: dict = field(default_factory=dict)


def redacted(cfg: ResolvedConfig) -> dict:
    """The resolved config as a loggable dict; secrets masked."""
    return {
        "api_key": encrypt(cfg.api_key),
        "avatar_id": cfg.avatar_id,
        "app_id": cfg.app_id,
        "environment": cfg.environment,
        "console_endpoint_url": cfg.console_endpoint_url,
        "ingress_endpoint_url": cfg.ingress_endpoint_url,
        "sample_rate": cfg.sample_rate,
        "channel": cfg.channel,
        "uid": cfg.uid,
        "agora_appid": cfg.agora_appid,
        "agora_appcert": encrypt(cfg.agora_appcert),
        "agora_token": encrypt(cfg.agora_token),
        "egress_idle_reconnect_ms": cfg.egress_idle_reconnect_ms,
    }


def redacted_params(channel: str, params: dict) -> dict:
    """The raw properties as a loggable dict, for the validation-failed log."""
    masked = {
        key: encrypt(str(value)) if key in SECRET_KEYS else value
        for key, value in (params or {}).items()
    }
    return {"channel": channel, "params": masked}


class _InputQueue:
    """Audio frames waiting for the sender, bounded by playout time (D4).

    When more than ``max_ms`` is queued the oldest frames are dropped; the
    caller is told once per overflow episode so the log is not flooded.
    """

    def __init__(self, max_ms: int = INPUT_QUEUE_MAX_MS) -> None:
        self._max_ms = max_ms
        self._frames: deque[tuple[bytes, int]] = deque()
        self._queued_ms = 0.0
        self._ready = asyncio.Event()
        self._overflowing = False

    def put(self, pcm: bytes, rate: int) -> bool:
        """Queue a frame; True when this put started dropping old frames."""
        self._frames.append((pcm, rate))
        self._queued_ms += duration_ms(pcm, rate)
        first_drop = False
        while self._queued_ms > self._max_ms and len(self._frames) > 1:
            old_pcm, old_rate = self._frames.popleft()
            self._queued_ms -= duration_ms(old_pcm, old_rate)
            if not self._overflowing:
                self._overflowing = True
                first_drop = True
        self._ready.set()
        return first_drop

    async def get(self) -> tuple[bytes, int]:
        while not self._frames:
            self._ready.clear()
            await self._ready.wait()
        pcm, rate = self._frames.popleft()
        self._queued_ms -= duration_ms(pcm, rate)
        if not self._frames:
            self._queued_ms = 0.0
            self._overflowing = False
        return pcm, rate

    def clear(self) -> int:
        count = len(self._frames)
        self._frames.clear()
        self._queued_ms = 0.0
        self._overflowing = False
        self._ready.clear()
        return count


class SpatialRealAvatarExtension(AsyncExtension):
    """Drives one SpatialReal avatar into the Agora channel from TTS audio."""

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: Optional[AsyncTenEnv] = None
        self.config: Optional[ResolvedConfig] = None
        self.client: Optional[SessionClient] = None
        self.pacer: Optional[Pacer] = None
        self._resampler: Optional[Resampler] = None
        self._queue = _InputQueue()
        self._connected = asyncio.Event()
        self._tasks: set[asyncio.Task] = set()
        self._sender_task: Optional[asyncio.Task] = None
        self._ticker_task: Optional[asyncio.Task] = None
        self._turn_id = 0
        self._flush_gen = 0
        self._stopping = False

    # -- lifecycle ----------------------------------------------------------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")
        self.ten_env = ten_env
        try:
            raw = await SpatialRealAvatarConfig.create_async(ten_env)
        except Exception as exc:
            ten_env.log_error(f"config: load failed: {traceback.format_exc()}")
            await self._send_error(
                CODE_CONFIG, type(exc).__name__, f"failed to load: {exc}"
            )
            return
        params = raw.params if isinstance(raw.params, dict) else {}
        try:
            self.config = resolve_config(raw.channel, params)
        except ValueError as exc:
            ten_env.log_error(
                f"config: invalid: {exc}; "
                f"config: {redacted_params(raw.channel, params)}"
            )
            await self._send_error(CODE_CONFIG, "ValueError", str(exc))
            return
        ten_env.log_info(f"config: {redacted(self.config)}")

        try:
            self._start_session(self.config)
        except Exception as exc:
            ten_env.log_error(f"session start failed: {traceback.format_exc()}")
            await self._send_error(CODE_FATAL, type(exc).__name__, str(exc))

    def _start_session(self, cfg: ResolvedConfig) -> None:
        settings = SessionSettings(
            api_key=cfg.api_key,
            avatar_id=cfg.avatar_id,
            app_id=cfg.app_id,
            environment=cfg.environment,
            console_endpoint_url=cfg.console_endpoint_url,
            ingress_endpoint_url=cfg.ingress_endpoint_url,
            sample_rate=cfg.sample_rate,
            channel=cfg.channel,
            uid=cfg.uid,
            agora_appid=cfg.agora_appid,
            agora_appcert=cfg.agora_appcert,
            agora_token=cfg.agora_token,
            egress_idle_reconnect_ms=cfg.egress_idle_reconnect_ms,
        )
        self.client = SessionClient(
            settings,
            on_report=self._on_report,
            on_connected=self._on_connected,
            on_disconnected=self._on_disconnected,
        )
        self.pacer = Pacer(sink=self.client, sample_rate=cfg.sample_rate)
        self._sender_task = self._spawn(self._sender_loop())
        self._ticker_task = self._spawn(self.pacer.run())
        self._spawn(self.client.start())

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
        self._stopping = True
        for task in (self._sender_task, self._ticker_task):
            if task is not None:
                task.cancel()
        self._sender_task = None
        self._ticker_task = None
        if self.client is not None:
            try:
                await self.client.stop()
            except Exception:
                ten_env.log_error(f"client stop: {traceback.format_exc()}")
        for task in list(self._tasks):
            task.cancel()
        self._queue.clear()

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    # -- dispatch -----------------------------------------------------------

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd name={cmd_name}")
        if cmd_name == "flush":
            try:
                await self._flush()
            except Exception:
                ten_env.log_error(f"flush: {traceback.format_exc()}")
            # D6: the flush continues downstream.
            await ten_env.send_cmd(Cmd.create("flush"))
        await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug(f"on_data name={data_name}")
        if self.pacer is None:
            return
        try:
            if data_name == "tts_audio_end":
                await self._on_tts_audio_end(ten_env, data)
            elif data_name == "finalize":
                await self._end_request("finalize")
        except Exception:
            ten_env.log_error(f"on_data {data_name}: {traceback.format_exc()}")

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        if self.pacer is None or self._stopping:
            return
        try:
            buf = audio_frame.get_buf()
            rate = audio_frame.get_sample_rate()
        except Exception:
            ten_env.log_error(f"audio frame: {traceback.format_exc()}")
            return
        if not buf:
            return
        if rate <= 0 and self.config is not None:
            rate = self.config.sample_rate
        if self._queue.put(bytes(buf), rate):
            ten_env.log_warn(
                f"input queue over {INPUT_QUEUE_MAX_MS} ms while the session "
                "is not up: dropping the oldest audio"
            )

    # -- request signals ----------------------------------------------------

    async def _on_tts_audio_end(self, ten_env: AsyncTenEnv, data: Data) -> None:
        payload = _payload(data)
        reason = payload.get("reason")
        if not isinstance(reason, int) or isinstance(reason, bool):
            # The pacer treats an unknown reason as "close"; keep what came
            # in out of the arithmetic and log it as such.
            reason = -1
        request_id = payload.get("request_id", "")
        metadata = payload.get("metadata") or {}
        turn_id = metadata.get("turn_id")
        if isinstance(turn_id, int) and not isinstance(turn_id, bool):
            self._turn_id = turn_id
        closed = await self.pacer.on_tts_audio_end(reason)
        self._note_request_state()
        if closed:
            self._reset_resampler()
            ten_env.log_info(
                f"request closed: tts_audio_end request_id={request_id} "
                f"reason={reason}"
            )
        else:
            ten_env.log_debug(
                f"tts_audio_end ignored: request_id={request_id} "
                f"reason={reason} request_open={self.pacer.request_open}"
            )

    async def _end_request(self, reason: str) -> None:
        closed = await self.pacer.end_request(reason)
        self._note_request_state()
        if closed:
            self._reset_resampler()
            self.ten_env.log_info(f"request closed: {reason}")

    async def _flush(self) -> None:
        self._flush_gen += 1
        dropped = self._queue.clear()
        if dropped:
            self.ten_env.log_debug(f"flush: dropped {dropped} queued frames")
        self._reset_resampler()
        if self.pacer is not None:
            await self.pacer.flush()
            self._note_request_state()

    def _note_request_state(self) -> None:
        if self.client is not None and self.pacer is not None:
            self.client.note_request_state(self.pacer.request_open)

    def _reset_resampler(self) -> None:
        if self._resampler is not None:
            self._resampler.reset()

    # -- sender -------------------------------------------------------------

    async def _sender_loop(self) -> None:
        """Resample queued frames and push them through the pacer.

        Waits for the session before sending, so audio queued during a
        reconnect goes out (bounded by the queue) instead of being lost.
        """
        while True:
            pcm, rate = await self._queue.get()
            gen = self._flush_gen
            await self._connected.wait()
            if gen != self._flush_gen:
                # A flush landed while this frame waited for the session:
                # it belongs to the cancelled turn.
                continue
            try:
                converted = self._resample(pcm, rate)
                if not converted:
                    continue
                await self.pacer.push_audio(converted)
                self._note_request_state()
            except Exception:
                # CancelledError derives from BaseException, so cancelling
                # the sender task still propagates past this handler.
                self.ten_env.log_error(f"send audio: {traceback.format_exc()}")

    def _resample(self, pcm: bytes, rate: int) -> bytes:
        dst = self.config.sample_rate
        if self._resampler is None or self._resampler.src_rate != rate:
            if self._resampler is not None:
                self.ten_env.log_info(
                    f"input sample rate changed "
                    f"{self._resampler.src_rate} -> {rate}"
                )
            self._resampler = Resampler(rate, dst)
        return self._resampler.convert(pcm)

    # -- session callbacks --------------------------------------------------

    def _on_connected(self, connection_id: str) -> None:
        self.ten_env.log_info(f"session connected: {connection_id}")
        if self.pacer is not None:
            self.pacer.on_session_reset()
        self._reset_resampler()
        self._note_request_state()
        self._connected.set()

    def _on_disconnected(self) -> None:
        self.ten_env.log_info("session disconnected")
        self._connected.clear()
        if self.pacer is not None:
            self.pacer.on_session_reset()
        self._reset_resampler()
        self._note_request_state()

    def _on_report(self, report: ErrorReport) -> None:
        self._spawn(
            self._send_error(report.code, report.vendor_code, report.message)
        )

    # -- errors -------------------------------------------------------------

    async def _send_error(
        self, code: int, vendor_code: str, message: str
    ) -> None:
        """Emit the D8 ``ModuleError`` on data ``error``."""
        if self.ten_env is None:
            return
        error = ModuleError(
            id=str(self._turn_id or 0),
            module=ModuleType.AVATAR.value,
            code=code,
            message=message,
            vendor_info=ModuleErrorVendorInfo(
                vendor=VENDOR, code=vendor_code, message=message
            ),
            metadata={
                "session_id": (
                    self.client.connection_id if self.client else ""
                ),
                "vendor_metadata": self._vendor_metadata(),
            },
        )
        try:
            data = Data.create("error")
            data.set_property_from_json(None, error.model_dump_json())
            line = f"error: code={code} vendor_code={vendor_code} {message}"
            if code == CODE_NON_FATAL:
                self.ten_env.log_warn(line)
            else:
                self.ten_env.log_error(line)
            await self.ten_env.send_data(data)
        except Exception:
            self.ten_env.log_error(f"send error: {traceback.format_exc()}")

    def _vendor_metadata(self) -> dict[str, Any]:
        """Identifiers only: never a URL, key or token; no empty values."""
        values: dict[str, Any] = {"name": VENDOR}
        if self.config is not None:
            values["avatar_id"] = self.config.avatar_id
            values["environment"] = self.config.environment
            values["agora_uid"] = self.config.uid
        return {k: v for k, v in values.items() if v not in ("", None, 0)}

    # -- tasks --------------------------------------------------------------

    def _spawn(self, coro) -> asyncio.Task:
        task = asyncio.ensure_future(coro)
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)
        return task


def _payload(data: Data) -> dict:
    json_str, _ = data.get_property_to_json(None)
    if not json_str:
        return {}
    try:
        payload = json.loads(json_str)
    except ValueError:
        return {}
    return payload if isinstance(payload, dict) else {}
