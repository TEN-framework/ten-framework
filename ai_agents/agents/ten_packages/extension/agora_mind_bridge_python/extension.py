"""TEN graph adapter for the Agora Mind Rust extension.

The bridge deliberately keeps the Agora Mind wire protocol in the existing
``agora_mind_extension_rust`` extension. It only translates ASR final results
to ``submit_user_input`` and completed top-level turns to TTS input.
"""

from __future__ import annotations

import asyncio
import json
from typing import Any

from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    CmdResult,
    Data,
    Loc,
    StatusCode,
)

from .config import AgoraMindBridgeConfig
from .completion import Completion
from .protocol import (
    asr_final_key,
    pure_text_input,
    session_id_from_response,
    speech_final_from_payload,
    input_sequence_from_response,
    turn_from_event,
)

FORWARDED_COMMANDS = {
    "create_workspace",
    "create_session",
    "flush_session",
    "set_session_debug",
    "delete_session",
    "finalize_audio",
}


class AgoraMindBridgeExtension(AsyncExtension):
    """Translate TEN ASR/TTS messages around Agora Mind's TEN extension."""

    def __init__(self, name: str):
        super().__init__(name)
        self.config = AgoraMindBridgeConfig()
        self.ten_env: AsyncTenEnv | None = None
        self.session_id: int | None = None
        self._last_final_text: str | None = None
        self._completed_turns: set[tuple[Any, ...]] = set()
        self._submitted_finals: set[tuple[int, int, str]] = set()
        self._session_epoch = 0
        self._pending_asr_text: list[str] = []
        self._pending_asr_created_at: Any = None
        self._finalize_id: str | None = None
        self._completion = Completion()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        config_json, error = await ten_env.get_property_to_json("")
        if error:
            raise ValueError(f"failed to read bridge properties: {error}")
        self.config = AgoraMindBridgeConfig.model_validate_json(config_json)
        self._reset_session(self.config.session_id)
        ten_env.log_info(
            "config: "
            + json.dumps(
                self.config.model_dump(exclude={"session_id"}),
                sort_keys=True,
            )
        )

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("Agora Mind bridge started")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        session_id = self.session_id
        self._reset_session()
        if session_id is not None:
            result, error = await self._send_command(
                "delete_session", {"session_id": session_id}
            )
            if (
                error is not None
                or result is None
                or result.get_status_code() != StatusCode.OK
            ):
                ten_env.log_warn("could not delete Agora Mind Session on stop")

    def _reset_session(self, session_id: int | None = None) -> None:
        self._session_epoch += 1
        self.session_id = session_id
        self._last_final_text = None
        self._completed_turns.clear()
        self._submitted_finals.clear()
        self._pending_asr_text.clear()
        self._pending_asr_created_at = None
        self._finalize_id = None
        self._completion = Completion()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        name = cmd.get_name()
        if name not in FORWARDED_COMMANDS:
            await self._return_error(
                ten_env, cmd, f"unsupported bridge command: {name}"
            )
            return

        payload, error = cmd.get_property_to_json(None)
        if error:
            await self._return_error(ten_env, cmd, str(error))
            return
        try:
            properties = json.loads(payload or "{}")
        except ValueError as error:
            await self._return_error(ten_env, cmd, str(error))
            return
        if not isinstance(properties, dict):
            await self._return_error(
                ten_env, cmd, "command properties must be an object"
            )
            return

        if name == "finalize_audio":
            if self.session_id is None:
                await self._return_error(ten_env, cmd, "Session is not ready")
                return
            finalize_id = properties.get("finalize_id")
            if not isinstance(finalize_id, str) or not finalize_id:
                await self._return_error(
                    ten_env, cmd, "finalize_id is required"
                )
                return
            if self._completion.finalize_id is not None:
                await self._return_error(
                    ten_env, cmd, "Audio finalize is pending"
                )
                return
            self._finalize_id = finalize_id
            self._completion.finalize_id = finalize_id
            data = Data.create("asr_finalize")
            data.set_dests([Loc("", "", self.config.stt_extension)])
            data.set_property_string("finalize_id", finalize_id)
            await ten_env.send_data(data)
            result = CmdResult.create(StatusCode.OK, cmd)
            result.set_property_string(
                "response_json",
                json.dumps(
                    {
                        "type": "audio.finalize.accepted",
                        "finalize_id": finalize_id,
                    }
                ),
            )
            await ten_env.return_result(result)
            return

        session_epoch = self._session_epoch
        result, send_error = await self._send_command(name, properties)
        if send_error is not None or result is None:
            await self._return_error(
                ten_env,
                cmd,
                str(send_error or "Agora Mind command returned no result"),
            )
            return
        if session_epoch != self._session_epoch:
            await self._return_error(
                ten_env, cmd, "Session changed while command was pending"
            )
            return

        response_json = self._result_property(result, "response_json")
        if (
            name == "create_session"
            and result.get_status_code() == StatusCode.OK
        ):
            self._reset_session(session_id_from_response(response_json or ""))
        elif (
            name in {"create_workspace", "delete_session"}
            and result.get_status_code() == StatusCode.OK
            and (
                name == "create_workspace"
                or properties.get("session_id") == self.session_id
            )
        ):
            self._reset_session()

        forwarded_result = CmdResult.create(result.get_status_code(), cmd)
        forwarded_result.set_property_string(
            "response_json",
            response_json
            or json.dumps(
                {
                    "type": "error",
                    "error": {
                        "kind": "bridge",
                        "message": "Agora Mind result omitted response_json",
                    },
                }
            ),
        )
        await ten_env.return_result(forwarded_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        name = data.get_name()
        payload_json, error = data.get_property_to_json(None)
        if error:
            ten_env.log_warn(f"bridge could not read {name}: {error}")
            return
        try:
            payload = json.loads(payload_json or "{}")
        except ValueError as error:
            ten_env.log_warn(f"bridge received invalid {name}: {error}")
            return
        if not isinstance(payload, dict):
            ten_env.log_warn(f"bridge received non-object {name}")
            return

        if name == "asr_result":
            await self._handle_asr_result(ten_env, payload)
        elif name == "asr_finalize_end":
            if (
                self._finalize_id is None
                or payload.get("finalize_id") != self._finalize_id
            ):
                return
            self._finalize_id = None
            self._completion.asr_finalized = True
            await self._submit_pending_asr(ten_env)
        elif name == "agora_mind_event":
            await self._handle_agora_event(ten_env, payload)
        await self._report_completion(ten_env)

    async def _report_completion(self, ten_env: AsyncTenEnv) -> None:
        completion = self._completion
        if self._pending_asr_text or not completion.ready():
            return
        completion.reported = True
        output = Data.create("processing_completed")
        output.set_property_from_json(
            None,
            json.dumps(
                {
                    "example_id": self.config.example_id,
                    "session_id": self.session_id,
                    "finalize_id": completion.finalize_id,
                    "last_input_sequence": max(
                        completion.accepted_inputs, default=0
                    ),
                    "tts_request_ids": completion.tts_request_ids,
                }
            ),
        )
        await ten_env.send_data(output)

    async def _submit_pending_asr(
        self, ten_env: AsyncTenEnv, suppress_repeated_text: bool = False
    ) -> None:
        if not self._pending_asr_text:
            return
        text = " ".join(self._pending_asr_text)
        created_at = self._pending_asr_created_at
        self._pending_asr_text.clear()
        self._pending_asr_created_at = None
        await self._submit_asr_text(
            ten_env, text, {"created_at": created_at}, suppress_repeated_text
        )

    async def _handle_asr_result(
        self, ten_env: AsyncTenEnv, payload: dict[str, Any]
    ) -> None:
        if payload.get("final") is not True:
            return
        if self.session_id is None:
            ten_env.log_warn("ASR final ignored before a Session is created")
            return
        text = payload.get("text")
        if not isinstance(text, str):
            return
        normalized = text.strip()
        key = asr_final_key(payload, normalized)
        if key is not None:
            if key in self._submitted_finals:
                return
            self._submitted_finals.add(key)
        if normalized and self._completion.asr_finalized:
            await self._report_input_error(
                ten_env,
                self.session_id,
                "ASR produced new text after finalization",
            )
            return
        metadata = payload.get("metadata")
        metadata = metadata if isinstance(metadata, dict) else {}
        if normalized:
            self._pending_asr_text.append(normalized)
            if self._pending_asr_created_at is None:
                self._pending_asr_created_at = metadata.get("created_at")
        if (
            not speech_final_from_payload(payload)
            or self._finalize_id is not None
        ):
            return
        await self._submit_pending_asr(ten_env, key is None)

    async def _submit_asr_text(
        self,
        ten_env: AsyncTenEnv,
        normalized: str,
        metadata: dict[str, Any],
        suppress_repeated_text: bool,
    ) -> None:
        if suppress_repeated_text and normalized == self._last_final_text:
            ten_env.log_debug("duplicate ASR final ignored")
            return
        if self.session_id is None:
            ten_env.log_warn("ASR final ignored before a Session is created")
            return

        input_payload = pure_text_input(normalized, metadata.get("created_at"))
        # Reserve before awaiting: concurrent duplicate finals and ambiguous
        # command timeouts must not resubmit an input that may be accepted.
        self._last_final_text = normalized
        session_id = self.session_id
        session_epoch = self._session_epoch
        completion = self._completion
        completion.pending_submissions += 1
        result, error = await self._send_command(
            "submit_user_input",
            {
                "session_id": session_id,
                "input_json": json.dumps(input_payload, ensure_ascii=False),
            },
        )
        completion.pending_submissions -= 1
        if session_epoch != self._session_epoch:
            return
        if error is not None or result is None:
            await self._report_input_error(
                ten_env,
                session_id,
                f"submit_user_input failed: {error or 'missing result'}",
            )
            return
        if result.get_status_code() != StatusCode.OK:
            await self._report_input_error(
                ten_env,
                session_id,
                "Agora Mind rejected ASR final",
                self._result_property(result, "response_json"),
            )
            return
        try:
            completion.accepted_inputs.add(
                input_sequence_from_response(
                    self._result_property(result, "response_json") or "",
                    session_id,
                )
            )
        except (ValueError, KeyError, TypeError) as error:
            await self._report_input_error(ten_env, session_id, str(error))

    async def _report_input_error(
        self,
        ten_env: AsyncTenEnv,
        session_id: int,
        message: str,
        response_json: str | None = None,
    ) -> None:
        self._completion.failed = True
        ten_env.log_error(message)
        metadata = {
            "session_id": str(session_id),
            "example_id": self.config.example_id,
        }
        if response_json is not None:
            metadata["response_json"] = response_json
        output = Data.create("error")
        output.set_property_from_json(
            None,
            json.dumps(
                {
                    "module": "agora_mind_bridge",
                    "code": 1000,  # TEN NON_FATAL_ERROR; the bridge stays usable.
                    "message": message,
                    "metadata": metadata,
                }
            ),
        )
        await ten_env.send_data(output)

    async def _handle_agora_event(
        self, ten_env: AsyncTenEnv, payload: dict[str, Any]
    ) -> None:
        event_json = payload.get("event_json")
        if not isinstance(event_json, str):
            ten_env.log_warn("agora_mind_event omitted event_json")
            return
        try:
            event_payload = json.loads(event_json)
        except ValueError:
            ten_env.log_warn("agora_mind_event contained invalid event_json")
            return
        if not isinstance(event_payload, dict):
            ten_env.log_warn("agora_mind_event was not a JSON object")
            return
        if event_payload.get("type") == "extension.connection_state":
            if event_payload.get("state") == "disconnected":
                self._reset_session()
            return
        event = event_payload.get("event")
        if (
            isinstance(event, dict)
            and event.get("type") in {"session_failed", "session_deleted"}
            and event.get("session_id") == self.session_id
        ):
            self._reset_session()
            return
        if self.session_id is None:
            return

        try:
            completed = turn_from_event(event, self.session_id)
        except ValueError as error:
            await self._report_input_error(ten_env, self.session_id, str(error))
            return
        if completed is None:
            return
        first, last, text = completed
        session_id = self.session_id
        key = (session_id, first, last)
        if key in self._completed_turns:
            ten_env.log_debug("duplicate Agora Mind turn ignored")
            return
        self.session_id = session_id
        turn_index = len(self._completed_turns) + 1
        completion = self._completion
        self._completed_turns.add(key)
        self._last_final_text = None
        if not text.strip():
            completion.completed_ranges.append((first, last))
            return
        request_id = f"fdb-tts-{session_id}-{turn_index}"
        completion.tts_request_ids.append(request_id)
        output = Data.create("tts_text_input")
        output.set_dests([Loc("", "", self.config.tts_extension)])
        output.set_property_from_json(
            None,
            json.dumps(
                {
                    "request_id": request_id,
                    "text": text,
                    "text_input_end": True,
                    "metadata": {
                        "request_id": request_id,
                        "session_id": str(session_id),
                        "example_id": self.config.example_id,
                        "turn_id": turn_index,
                    },
                },
                ensure_ascii=False,
            ),
        )
        await ten_env.send_data(output)
        completion.completed_ranges.append((first, last))

    async def _send_command(
        self, name: str, properties: dict[str, Any]
    ) -> tuple[Any, Any]:
        if self.ten_env is None:
            return None, RuntimeError("bridge is not initialized")
        command = Cmd.create(name)
        command.set_dests([Loc("", "", self.config.agora_mind_extension)])
        command.set_property_from_json(None, json.dumps(properties))
        try:
            return await asyncio.wait_for(
                self.ten_env.send_cmd(command),
                timeout=self.config.command_timeout_ms / 1000,
            )
        except asyncio.TimeoutError:
            return None, TimeoutError(f"{name} timed out")
        except Exception as error:
            return None, error

    async def _return_error(
        self, ten_env: AsyncTenEnv, cmd: Cmd, detail: str
    ) -> None:
        result = CmdResult.create(StatusCode.ERROR, cmd)
        result.set_property_string(
            "response_json",
            json.dumps(
                {
                    "type": "error",
                    "error": {"kind": "bridge", "message": detail},
                }
            ),
        )
        await ten_env.return_result(result)

    @staticmethod
    def _result_property(result: Any, name: str) -> str | None:
        try:
            value = result.get_property_string(name)
        except (AttributeError, TypeError):
            return None
        if isinstance(value, tuple):
            value, error = value
            if error:
                return None
        return value if isinstance(value, str) else None
