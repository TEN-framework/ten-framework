#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""TEN glue for the SpatialReal avatar.

Everything TEN-specific lives here: the property dataclass, the hook
dispatch (audio frames, ``tts_audio_end``/``finalize`` data, the ``flush``
command) and ``ModuleError`` emission (design D8). The work is delegated to
the pure modules: ``config`` validates the properties, ``sender`` queues
frames and end markers in order and resamples to the session rate,
``pacer`` owns request boundaries and padding, and ``session_client`` keeps
the SpatialReal session up.

Nothing raises out of a hook (an uncaught exception ends the TEN process):
a configuration failure is reported as a ``-1012`` error and the extension
stays idle; a session failure is reported by the client and the audio path
simply has nowhere to send until it reconnects.
"""

import asyncio
import json
import traceback
from dataclasses import asdict, dataclass, field
from typing import Any, Mapping, Optional

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

from .config import ResolvedConfig, resolve_config
from .pacer import Pacer
from .sender import AudioSender, End, QueueCapacityError, Turn
from .session_client import (
    CODE_CONFIG,
    CODE_FATAL,
    CODE_NON_FATAL,
    ErrorReport,
    SessionClient,
    SessionSettings,
)

VENDOR = "spatialreal"
# Property keys whose values are secrets; redacted wherever config is logged.
SECRET_KEYS = frozenset({"spatialreal_api_key", "agora_token", "agora_appcert"})
# ``ResolvedConfig`` fields whose name differs from their property key.
_PROPERTY_KEY_OF_FIELD = {"api_key": "spatialreal_api_key"}


@dataclass
class SpatialRealAvatarConfig(BaseConfig):
    """The two top-level properties (D3): the server-owned channel and the
    operator's ``params`` object. Every other key lives inside ``params``."""

    channel: str = ""
    params: dict = field(default_factory=dict)


def _masked(values: Mapping[str, Any], property_key_of=None) -> dict:
    """``values`` with every ``SECRET_KEYS`` entry replaced by its mask."""
    property_key_of = property_key_of or (lambda name: name)
    return {
        name: (
            encrypt(str(value))
            if property_key_of(name) in SECRET_KEYS
            else value
        )
        for name, value in values.items()
    }


def redacted(cfg: ResolvedConfig) -> dict:
    """The resolved config as a loggable dict; secrets masked."""
    return _masked(
        asdict(cfg), lambda name: _PROPERTY_KEY_OF_FIELD.get(name, name)
    )


def redacted_params(channel: str, params: dict) -> dict:
    """The raw properties as a loggable dict, for the validation-failed log."""
    return {"channel": channel, "params": _masked(params or {})}


class SpatialRealAvatarExtension(AsyncExtension):
    """Drives one SpatialReal avatar into the Agora channel from TTS audio."""

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: Optional[AsyncTenEnv] = None
        self.config: Optional[ResolvedConfig] = None
        self.client: Optional[SessionClient] = None
        self.pacer: Optional[Pacer] = None
        self.sender: Optional[AudioSender] = None
        self._tasks: set[asyncio.Task] = set()
        self._sender_task: Optional[asyncio.Task] = None
        self._ticker_task: Optional[asyncio.Task] = None
        self._turn_id = 0
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
        # ResolvedConfig and SessionSettings share their field names 1:1.
        settings = SessionSettings(**asdict(cfg))
        self.client = SessionClient(
            settings,
            on_report=self._on_report,
            on_connected=self._on_connected,
            on_disconnected=self._on_disconnected,
        )
        self.pacer = Pacer(sink=self.client, sample_rate=cfg.sample_rate)
        self.sender = AudioSender(
            self.pacer,
            cfg.sample_rate,
            self.ten_env,
            on_request_state=self.client.note_request_state,
        )
        self._sender_task = self._spawn(self.sender.run())
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
        if self.sender is not None:
            self.sender.clear()

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    # -- dispatch -----------------------------------------------------------

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd name={cmd_name}")
        if cmd_name == "flush":
            try:
                if self.sender is not None:
                    await self.sender.flush()
            except Exception:
                ten_env.log_error(f"flush: {traceback.format_exc()}")
            # D6: the flush continues downstream.
            await ten_env.send_cmd(Cmd.create("flush"))
        await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug(f"on_data name={data_name}")
        if self.sender is None:
            return
        try:
            if data_name == "tts_audio_end":
                self._on_tts_audio_end(ten_env, data)
            elif data_name == "finalize":
                payload = _payload(data)
                self.sender.put_end(
                    End(
                        "finalize",
                        turn=Turn.from_metadata(payload.get("metadata")),
                        request_id=_request_id(payload),
                    )
                )
        except QueueCapacityError as exc:
            await self._handle_queue_capacity(exc)
        except Exception:
            ten_env.log_error(f"on_data {data_name}: {traceback.format_exc()}")

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        if self.sender is None or self._stopping:
            return
        try:
            buf = audio_frame.get_buf()
            rate = audio_frame.get_sample_rate()
            turn = Turn.from_metadata(_payload(audio_frame, "metadata"))
        except Exception:
            ten_env.log_error(f"audio frame: {traceback.format_exc()}")
            return
        if not buf:
            return
        if rate <= 0 and self.config is not None:
            rate = self.config.sample_rate
        try:
            self.sender.put_audio(bytes(buf), rate, turn)
        except QueueCapacityError as exc:
            await self._handle_queue_capacity(exc)

    async def _handle_queue_capacity(self, error: QueueCapacityError) -> None:
        """Fail closed before yielding; never acknowledge a shortened turn."""
        turn = error.turn
        try:
            try:
                if self.sender is not None:
                    # flush retires and clears synchronously before its first
                    # await, so subsequent hooks cannot race old PCM/end past it.
                    await self.sender.flush()
            except Exception:
                self.ten_env.log_error("input queue overflow: interrupt failed")
            if not self._stopping:
                await self._send_error(
                    CODE_NON_FATAL,
                    "InputQueueOverflow",
                    f"{error}; buffered speech cancelled",
                    session_id=turn.session_id if turn is not None else None,
                    turn_id=turn.turn_id if turn is not None else None,
                )
        except asyncio.CancelledError:
            # TEN treats an uncaught BaseException in a hook as process fatal.
            # Cancellation already retired the old turn; stopping must neither
            # kill the worker nor manufacture a successful normal end.
            return

    # -- request signals ----------------------------------------------------

    def _on_tts_audio_end(self, ten_env: AsyncTenEnv, data: Data) -> None:
        """Queue the end as a marker behind the audio already queued.

        The pacer applies the D5 reason rules when the marker is reached;
        the turn id is taken at once because it only labels error reports.
        """
        payload = _payload(data)
        reason = payload.get("reason")
        if not isinstance(reason, int) or isinstance(reason, bool):
            # The pacer treats an unknown reason as "close"; keep what came
            # in out of the arithmetic and log it as such.
            reason = -1
        request_id = _request_id(payload)
        turn = Turn.from_metadata(payload.get("metadata"))
        if turn is not None:
            self._turn_id = turn.turn_id
        label = f"tts_audio_end request_id={request_id} reason={reason}"
        ten_env.log_debug(f"queued end: {label}")
        self.sender.put_end(End(label, reason, turn, request_id))

    # -- session callbacks --------------------------------------------------

    def _on_connected(self, connection_id: str) -> None:
        self.ten_env.log_info(f"session connected: {connection_id}")
        if self.sender is not None:
            self.sender.session_up()

    def _on_disconnected(self) -> None:
        self.ten_env.log_info("session disconnected")
        if self.sender is not None:
            self.sender.session_down()

    def _on_report(self, report: ErrorReport) -> None:
        # The report carries the id of the session it is about; by the time
        # the spawned task runs, the client may already have cleared its own.
        self._spawn(
            self._send_error(
                report.code,
                report.vendor_code,
                report.message,
                session_id=report.session_id,
            )
        )

    # -- errors -------------------------------------------------------------

    async def _send_error(
        self,
        code: int,
        vendor_code: str,
        message: str,
        session_id: Optional[str] = None,
        turn_id: Optional[int] = None,
    ) -> None:
        """Emit the D8 ``ModuleError`` on data ``error``.

        ``session_id`` defaults to the client's current connection id; a
        report from the client names the session it is about instead.
        """
        if self.ten_env is None:
            return
        if session_id is None:
            session_id = self.client.connection_id if self.client else ""
        error = ModuleError(
            id=str(turn_id if turn_id is not None else self._turn_id or 0),
            module=ModuleType.AVATAR.value,
            code=code,
            message=message,
            vendor_info=ModuleErrorVendorInfo(
                vendor=VENDOR, code=vendor_code, message=message
            ),
            metadata={
                "session_id": session_id,
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


def _request_id(payload: dict) -> str:
    value = payload.get("request_id", "")
    return value if isinstance(value, str) else ""


def _payload(data, path=None) -> dict:
    json_str, _ = data.get_property_to_json(path)
    if not json_str:
        return {}
    try:
        payload = json.loads(json_str)
    except ValueError:
        return {}
    return payload if isinstance(payload, dict) else {}
