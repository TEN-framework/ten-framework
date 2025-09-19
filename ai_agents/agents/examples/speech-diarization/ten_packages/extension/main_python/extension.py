#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import json
import time
from typing import Dict

from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    CmdResult,
    Data,
    Loc,
    StatusCode,
)

from .config import DiarizationControlConfig


class DiarizationControlExtension(AsyncExtension):
    """Formats Speechmatics diarization output for the TEN message collector."""

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.config = DiarizationControlConfig()
        self._speaker_stream_ids: Dict[str, int] = {}
        self._next_stream_id: int = 1

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        config_json, _ = await ten_env.get_property_to_json(None)
        if config_json:
            self.config = DiarizationControlConfig.model_validate_json(
                config_json
            )

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        self._speaker_stream_ids.clear()
        self._next_stream_id = 1

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        # Acknowledge standard RTC lifecycle commands to keep logs clean.
        try:
            result = CmdResult.create(StatusCode.OK, cmd)
            result.set_property_string("detail", "ack")
            await ten_env.return_result(result)
        except Exception as exc:  # pragma: no cover - defensive
            ten_env.log_error(f"Failed to ack command {cmd.get_name()}: {exc}")

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() != "asr_result":
            ten_env.log_debug(
                f"Ignoring unsupported data message: {data.get_name()}"
            )
            return

        asr_json, err = data.get_property_to_json(None)
        if err:
            ten_env.log_error(f"Invalid ASR payload: {err}")
            return

        asr = json.loads(asr_json)
        if not asr:
            return

        final = bool(asr.get("final", False))
        if (not final) and self.config.skip_partials:
            return

        diarization = (
            asr.get("metadata", {}).get("diarization", {}) or {}
        )
        segments = diarization.get("segments") or []

        if not segments:
            text = (asr.get("text") or "").strip()
            if text:
                await self._send_transcript(
                    text=text,
                    is_final=final,
                    stream_id=self._get_stream_id("UU", None),
                    speaker="UU",
                    channel=None,
                    start_ms=asr.get("start_ms"),
                    end_ms=asr.get("start_ms", 0)
                    + asr.get("duration_ms", 0),
                )
            return

        for segment in segments:
            text = (segment.get("text") or "").strip()
            if not text:
                continue

            speaker = segment.get("speaker") or "UU"
            channel = segment.get("channel")
            stream_id = self._get_stream_id(speaker, channel)
            decorated_text = self._format_segment_text(speaker, channel, text)

            await self._send_transcript(
                text=decorated_text,
                is_final=final,
                stream_id=stream_id,
                speaker=speaker,
                channel=channel,
                start_ms=segment.get("start_ms"),
                end_ms=segment.get("end_ms"),
                duration_ms=segment.get("duration_ms"),
            )

    def _get_stream_id(self, speaker: str, channel: str | None) -> int:
        key = f"{channel or ''}:{speaker}"
        if key not in self._speaker_stream_ids:
            self._speaker_stream_ids[key] = self._next_stream_id
            self._next_stream_id += 1
        return self._speaker_stream_ids[key]

    def _format_segment_text(
        self, speaker: str, channel: str | None, text: str
    ) -> str:
        if not self.config.speaker_prefix:
            return text

        label = speaker
        if channel and self.config.show_channel_labels:
            label = f"{speaker}@{channel}"
        prefix = self.config.speaker_prefix.format(label)
        return f"{prefix}{text}"

    async def _send_transcript(
        self,
        text: str,
        is_final: bool,
        stream_id: int,
        speaker: str,
        channel: str | None,
        start_ms: int | None,
        end_ms: int | None,
        duration_ms: int | None = None,
    ) -> None:
        if not self.ten_env:
            return

        timestamp_ms = int(time.time() * 1000)
        metadata = {
            "speaker": speaker,
            "channel": channel,
            "start_ms": start_ms,
            "end_ms": end_ms,
            "duration_ms": duration_ms,
        }

        payload = {
            "data_type": "transcribe",
            "role": "user",
            "text": text,
            "text_ts": timestamp_ms,
            "is_final": is_final,
            "stream_id": stream_id,
            "metadata": metadata,
        }

        await self._send_data("message", "message_collector", payload)

    async def _send_data(
        self, data_name: str, dest: str, payload: dict
    ) -> None:
        data = Data.create(data_name)
        data.set_dests([Loc("", "", dest)])
        data.set_property_from_json(None, json.dumps(payload))
        await self.ten_env.send_data(data)
