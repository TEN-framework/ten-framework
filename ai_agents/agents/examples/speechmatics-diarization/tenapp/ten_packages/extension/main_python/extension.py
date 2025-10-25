import json
import time
from typing import Literal, Optional

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
)

from .agent.agent import Agent
from .agent.events import (
    ASRResultEvent,
    LLMResponseEvent,
    ToolRegisterEvent,
    UserJoinedEvent,
    UserLeftEvent,
)
from .helper import _send_cmd, _send_data, parse_sentences
from .config import MainControlConfig  # assume extracted from your base model
from .game_logic import WhoLikesWhatGame

import uuid


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module.
    Consumes semantic AgentEvents from the Agent class and drives the runtime behavior.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None
        self.agent: Agent = None
        self.config: MainControlConfig = None

        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"
        self.pending_response_target: Optional[str] = None
        self.game: Optional[WhoLikesWhatGame] = None

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        self.agent = Agent(ten_env)
        self.game = WhoLikesWhatGame(self)

        # Now auto-register decorated methods
        for attr_name in dir(self):
            fn = getattr(self, attr_name)
            event_type = getattr(fn, "_agent_event_type", None)
            if event_type:
                self.agent.on(event_type, fn)

    # === Register handlers with decorators ===
    @agent_event_handler(UserJoinedEvent)
    async def _on_user_joined(self, event: UserJoinedEvent):
        self._rtc_user_count += 1
        if self._rtc_user_count == 1 and self.config and self.config.greeting:
            await self._send_to_tts(self.config.greeting, True)
            # No label for assistant greeting
            await self._send_transcript(
                "assistant", self.config.greeting, True, 100
            )
        if self.game and not self.game.enrollment_prompted:
            await self.game.start_enrollment_flow()

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, event: UserLeftEvent):
        self._rtc_user_count -= 1

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
        game = self.game
        if not game:
            return

        raw_session_id = event.metadata.get("session_id", "100")
        self.session_id = str(raw_session_id)
        stream_id = 100
        for candidate in (
            event.metadata.get("stream_id"),
            raw_session_id,
        ):
            try:
                if candidate is not None:
                    stream_id = int(candidate)
                    break
            except (TypeError, ValueError):
                continue
        else:
            self.ten_env.log_warn(
                f"[ASR] Unable to parse stream_id from metadata; defaulting to {stream_id}. metadata={event.metadata}"
            )

        # Extract speaker information for diarization
        speaker = event.metadata.get("speaker", "")
        channel = event.metadata.get("channel", "")
        speaker_str = game.normalize_label(speaker)
        channel_str = game.normalize_label(channel)
        speaker_key = game.build_speaker_key(speaker_str, channel_str)

        # Debug logging to check if speaker info is received
        if event.final:
            self.ten_env.log_info(
                f"[ASR] Received metadata: speaker='{speaker}', channel='{channel}', metadata={event.metadata}"
            )

        # Format speaker label as [S1], [S2], etc.
        speaker_label = ""
        assigned_name = (
            game.speaker_assignments[speaker_key]
            if speaker_key and speaker_key in game.speaker_assignments
            else None
        )
        if assigned_name:
            speaker_label = f"[{assigned_name}] "
            self.ten_env.log_info(
                f"[ASR] Using enrolled label: {speaker_label}"
            )
        elif speaker_str:
            speaker_label = f"[{speaker_str}] "
            self.ten_env.log_info(f"[ASR] Using speaker label: {speaker_label}")
        elif channel_str:
            speaker_label = f"[{channel_str}] "
            self.ten_env.log_info(f"[ASR] Using channel label: {speaker_label}")
        else:
            # If no speaker/channel info, use last known speaker or default
            if game.last_speaker:
                speaker_label = f"[{game.last_speaker}] "
                self.ten_env.log_info(
                    f"[ASR] Using last speaker label: {speaker_label}"
                )
            else:
                speaker_label = "[USER] "
                self.ten_env.log_info(
                    f"[ASR] Using default label: {speaker_label}"
                )

        if not event.text:
            return
        if event.final or len(event.text) > 2:
            await self._interrupt()
        queue_text: Optional[str] = None
        if event.final:
            self.turn_id += 1
            # Track the current speaker
            resolved_label = speaker_str if speaker_str else channel_str
            registered_name = await game.assign_player_if_needed(
                speaker_key, event.text, not game.enrollment_complete
            )
            if registered_name:
                assigned_name = registered_name
                speaker_label = f"[{assigned_name}] "
            elif speaker_key and speaker_key in game.speaker_assignments:
                assigned_name = game.speaker_assignments[speaker_key]
                speaker_label = f"[{assigned_name}] "
            if assigned_name:
                resolved_label = assigned_name
            if resolved_label:
                game.last_speaker = resolved_label

            if not game.enrollment_complete:
                await game.handle_enrollment_stage(
                    assigned_name or resolved_label,
                    speaker_key,
                    event.text,
                )
            else:
                handled = await game.handle_game_flow(
                    assigned_name or resolved_label, event.text
                )
                if not handled and not assigned_name:
                    await game.handle_unknown_speaker(event.text)
                queue_text = None

        if queue_text:
            await self.agent.queue_llm_input(queue_text)

        # Add speaker label to transcript display (always include label)
        transcript_text = f"{speaker_label}{event.text}"
        self.ten_env.log_info(f"[ASR] Sending transcript: {transcript_text}")
        await self._send_transcript(
            "user", transcript_text, event.final, stream_id
        )

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        target_player = self.pending_response_target
        if not event.is_final and event.type == "message":
            sentences, self.sentence_fragment = parse_sentences(
                self.sentence_fragment, event.delta
            )
            for s in sentences:
                if target_player:
                    await self._send_to_tts(s, False, target_player)

        if event.is_final and event.type == "message":
            remaining_text = self.sentence_fragment or ""
            self.sentence_fragment = ""
            if target_player and remaining_text:
                await self._send_to_tts(remaining_text, True, target_player)
            # Clear target when the turn is done
            self.pending_response_target = None

        # No label for assistant responses
        display_text = event.text
        if target_player and display_text:
            display_text = f"[{target_player}] {display_text}"
        await self._send_transcript(
            "assistant",
            display_text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        self.pending_response_target = None
        if self.game:
            self.game.reset_state()
        ten_env.log_info("[MainControlExtension] stopping agent...")
        await self.agent.stop()
        ten_env.log_info("[MainControlExtension] agent stopped")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        await self.agent.on_data(data)

    # === helpers ===
    async def _send_transcript(
        self,
        role: str,
        text: str,
        final: bool,
        stream_id: int,
        data_type: Literal["text", "reasoning"] = "text",
    ):
        """
        Sends the transcript (ASR or LLM output) to the message collector.
        """
        if data_type == "text":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "transcribe",
                    "role": role,
                    "text": text,
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        elif data_type == "reasoning":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "raw",
                    "role": role,
                    "text": json.dumps(
                        {
                            "type": "reasoning",
                            "data": {
                                "text": text,
                            },
                        }
                    ),
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
        )

    async def _send_to_tts(
        self, text: str, is_final: bool, target_player: Optional[str] = None
    ):
        """
        Sends a sentence to the TTS system.
        """
        request_id = f"tts-request-{self.turn_id}-{uuid.uuid4().hex[:8]}"
        metadata = self._current_metadata()
        if target_player:
            metadata = {**metadata, "target_player": target_player}
        await _send_data(
            self.ten_env,
            "tts_text_input",
            "tts",
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": is_final,
                "metadata": metadata,
            },
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}"
        )

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        self.sentence_fragment = ""
        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")

    def _player_pronoun(self, player_name: str) -> tuple[str, str]:
        pronoun_map = {
            "Elliot": ("he", "loves"),
            "Musk": ("he", "loves"),
            "Taytay": ("she", "loves"),
        }
        return pronoun_map.get(player_name, ("they", "love"))
