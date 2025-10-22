import asyncio
import json
import re
import string
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

import uuid


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module.
    Consumes semantic AgentEvents from the Agent class and drives the runtime behavior.
    """

    PLAYER_ALIAS_MAP: dict[str, list[str]] = {
        "Elliot": ["elliot", "elliott", "elyot"],
        "Musk": ["musk", "elon", "mass", "mask"],
        "Taytay": ["taytay", "tay tay", "tate", "taylor", "swift", "tay"],
    }

    INTRODUCTION_PATTERNS: list[str] = [
        r"\bthis is\s+{alias}\b",
        r"\bi['\s]*m\s+{alias}\b",
        r"\bi am\s+{alias}\b",
        r"\bit['\s]*s\s+{alias}\b",
        r"\bmy name is\s+{alias}\b",
        r"\bname['\s]*s\s+{alias}\b",
    ]

    GREETING_TEMPLATES: set[str] = {
        "hello {alias}",
        "hi {alias}",
        "hey {alias}",
        "{alias} here",
    }

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
        self.last_speaker: str = ""  # Track the last speaker for context
        # === Game state for "Who Likes What" ===
        self.player_names: list[str] = ["Elliot", "Musk", "Taytay"]
        self.speaker_assignments: dict[str, str] = {}
        self.enrollment_prompted: bool = False
        self.enrollment_complete: bool = False
        self.pending_response_target: Optional[str] = None
        self.last_unknown_speaker_ts: float = 0.0
        self.last_turn_reminder_ts: dict[str, float] = {}
        self.game_stage: str = "enrollment"
        self.food_preferences: dict[str, str] = {}
        self.questions_answered: set[str] = set()
        self.enrollment_order: list[str] = list(self.player_names)
        self.enrollment_index: int = 0
        self.completed_enrollments: set[str] = set()
        self.awaiting_additional_request: bool = False

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        self.agent = Agent(ten_env)

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
        if not self.enrollment_prompted:
            await self._start_enrollment_flow()

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, event: UserLeftEvent):
        self._rtc_user_count -= 1

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
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
        speaker_str = self._normalize_label(speaker)
        channel_str = self._normalize_label(channel)
        speaker_key = self._build_speaker_key(speaker_str, channel_str)

        # Debug logging to check if speaker info is received
        if event.final:
            self.ten_env.log_info(
                f"[ASR] Received metadata: speaker='{speaker}', channel='{channel}', metadata={event.metadata}"
            )

        # Format speaker label as [S1], [S2], etc.
        speaker_label = ""
        assigned_name = (
            self.speaker_assignments[speaker_key]
            if speaker_key and speaker_key in self.speaker_assignments
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
            if self.last_speaker:
                speaker_label = f"[{self.last_speaker}] "
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
            registered_name = await self._assign_player_if_needed(
                speaker_key, event.text, not self.enrollment_complete
            )
            if registered_name:
                assigned_name = registered_name
                speaker_label = f"[{assigned_name}] "
            elif speaker_key and speaker_key in self.speaker_assignments:
                assigned_name = self.speaker_assignments[speaker_key]
                speaker_label = f"[{assigned_name}] "
            if assigned_name:
                resolved_label = assigned_name
            if resolved_label:
                self.last_speaker = resolved_label

            if not self.enrollment_complete:
                await self._handle_enrollment_stage(
                    assigned_name or resolved_label,
                    speaker_key,
                    event.text,
                )
            else:
                handled = await self._handle_game_flow(
                    assigned_name or resolved_label, event.text
                )
                if not handled and not assigned_name:
                    await self._handle_unknown_speaker(event.text)
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

    @staticmethod
    def _normalize_label(value) -> str:
        if value is None:
            return ""
        if isinstance(value, str):
            value = value.strip()
        if value == "":
            return ""
        return str(value).upper()

    def _build_speaker_key(self, speaker: str, channel: str) -> str:
        if speaker:
            return f"speaker:{speaker}"
        if channel:
            return f"channel:{channel}"
        return ""

    @staticmethod
    def _detect_declared_player(text: str) -> Optional[str]:
        if not text:
            return None
        lowered = text.lower()
        stripped = lowered.strip(string.whitespace + string.punctuation)
        for player, aliases in MainControlExtension.PLAYER_ALIAS_MAP.items():
            for alias in aliases:
                alias_lower = alias.lower()
                # Direct match such as "Elliot" or "Elliot."
                if stripped == alias_lower:
                    return player
                # Greeting phrases like "Hello Elliot"
                for template in MainControlExtension.GREETING_TEMPLATES:
                    if stripped == template.format(alias=alias_lower):
                        return player
                # Introduction statements
                escaped_alias = re.escape(alias_lower)
                for pattern in MainControlExtension.INTRODUCTION_PATTERNS:
                    if re.search(pattern.format(alias=escaped_alias), lowered):
                        return player
        return None

    @staticmethod
    def _normalize_food_text(text: str) -> str:
        if not text:
            return text

        working = text.strip()
        lowered = working.lower()

        leading_patterns = [
            r"^i\s+really\s+like\s+",
            r"^i\s+really\s+love\s+",
            r"^i\s+really\s+enjoy\s+",
            r"^i\s+like\s+",
            r"^i\s+love\s+",
            r"^i\s+enjoy\s+",
            r"^i['\s]*m\s+into\s+",
            r"^i\s+am\s+into\s+",
            r"^my\s+(favorite|favourite)\s+(food\s+)?(is|would be)\s+",
            r"^favorite\s+(food\s+)?(is|would be)\s+",
        ]

        for pattern in leading_patterns:
            if re.match(pattern, lowered):
                span = re.match(pattern, lowered).span()
                working = working[span[1]:]
                break

        # Strip lingering helper words like "to eat" that can remain after the leading pattern removal.
        def _strip_prefix(text: str) -> str:
            prefixes = [
                "to eat ",
                "to eat",
                "eat ",
                "eat",
                "to ",
                "to",
            ]
            trimmed = text
            changed = True
            while changed:
                changed = False
                lowered_text = trimmed.lower()
                for prefix in prefixes:
                    if lowered_text.startswith(prefix):
                        trimmed = trimmed[len(prefix):]
                        changed = True
                        break
            return trimmed

        working = _strip_prefix(working.lstrip())

        working = working.strip(string.whitespace + string.punctuation)
        return working or text.strip()

    @staticmethod
    def _looks_like_reassignment(
        text: str, current_name: str, new_name: str, enrollment_active: bool
    ) -> bool:
        if not text:
            return False

        lowered = text.lower()
        stripped = lowered.strip(string.whitespace + string.punctuation)
        current = current_name.lower()
        candidate = new_name.lower()

        correction_markers = [
            "actually",
            "sorry",
            "correction",
            "i mean",
        ]
        if any(marker in lowered for marker in correction_markers):
            return True

        negatives = [
            f"not {current}",
            f"no {current}",
            f"isn't {current}",
            f"i'm {candidate} not {current}",
            f"i am {candidate} not {current}",
        ]
        if any(pattern in lowered for pattern in negatives):
            return True

        if enrollment_active:
            aliases = MainControlExtension.PLAYER_ALIAS_MAP.get(
                new_name, [new_name]
            )
            for alias in aliases:
                alias_lower = alias.lower()
                if stripped == alias_lower:
                    return True
                for template in MainControlExtension.GREETING_TEMPLATES:
                    if stripped == template.format(alias=alias_lower):
                        return True
                escaped_alias = re.escape(alias_lower)
                for pattern in MainControlExtension.INTRODUCTION_PATTERNS:
                    if re.search(pattern.format(alias=escaped_alias), lowered):
                        return True

        explicit_phrases = [
            f"my name is {candidate}",
            f"this is {candidate}",
            f"i am {candidate}",
            f"i'm {candidate}",
            f"it's {candidate}",
            f"{candidate} here",
        ]
        if any(pattern in lowered for pattern in explicit_phrases) and (
            "not" in lowered or "actually" in lowered
        ):
            return True

        return False

    async def _start_enrollment_flow(self):
        """
        Kick off the enrollment flow by prompting each player one at a time.
        """
        if self.enrollment_prompted:
            return
        self.enrollment_prompted = True
        self.enrollment_complete = False
        self.enrollment_index = 0
        self.completed_enrollments.clear()
        self.speaker_assignments.clear()
        self.last_turn_reminder_ts.clear()
        await self._prompt_current_enrollment()

    async def _prompt_current_enrollment(self):
        if self.enrollment_index >= len(self.enrollment_order):
            return
        player_name = self.enrollment_order[self.enrollment_index]
        self.game_stage = f"enrollment_{player_name.lower()}"
        prompt = f"{player_name}, please say hello so I can learn your voice."
        await self._send_transcript("assistant", prompt, True, 100)
        await self._send_to_tts(prompt, True, player_name)

    async def _handle_enrollment_stage(
        self,
        speaker: Optional[str],
        speaker_key: str,
        transcript_text: str,
    ):
        if self.enrollment_index >= len(self.enrollment_order):
            return

        expected_player = self.enrollment_order[self.enrollment_index]
        if speaker != expected_player:
            self.ten_env.log_info(
                f"[Enrollment] Received speech from {speaker or 'unknown'} while awaiting {expected_player}"
            )
            return

        if expected_player in self.completed_enrollments:
            return

        self.completed_enrollments.add(expected_player)
        await self._announce_enrollment(expected_player)
        self.enrollment_index += 1

        if self.enrollment_index < len(self.enrollment_order):
            await self._prompt_current_enrollment()
        else:
            self.enrollment_complete = True
            await self._announce_enrollment_completion()

    async def _assign_player_if_needed(
        self,
        speaker_key: str,
        transcript_text: str = "",
        allow_reassignment: bool = True,
    ) -> Optional[str]:
        """
        Attach the diarization speaker key to the next available player name.
        """
        if not speaker_key:
            return None
        declared = (
            self._detect_declared_player(transcript_text) if allow_reassignment else None
        )

        existing = self.speaker_assignments.get(speaker_key)
        if existing:
            if allow_reassignment and declared and declared != existing:
                if not self._looks_like_reassignment(
                    transcript_text,
                    existing,
                    declared,
                    not self.enrollment_complete,
                ):
                    self.ten_env.log_info(
                        f"[Enrollment] Ignoring mention of {declared} from already registered {existing}"
                    )
                    return existing
                for key, value in list(self.speaker_assignments.items()):
                    if key != speaker_key and value == declared:
                        del self.speaker_assignments[key]
                        break
                self.speaker_assignments[speaker_key] = declared
                self.ten_env.log_info(
                    f"[Enrollment] Corrected {speaker_key} -> {declared}"
                )
                if not self.enrollment_complete:
                    await self._announce_enrollment(declared)
                    if len(self.speaker_assignments) == len(self.player_names):
                        self.enrollment_complete = True
                        await self._announce_enrollment_completion()
                return declared
            return existing

        if len(self.speaker_assignments) >= len(self.player_names):
            return None

        candidate: Optional[str] = None
        if allow_reassignment and declared and declared not in self.speaker_assignments.values():
            candidate = declared
        else:
            for name in self.player_names:
                if name not in self.speaker_assignments.values():
                    candidate = name
                    break

        if not candidate:
            return None

        self.speaker_assignments[speaker_key] = candidate
        self.ten_env.log_info(
            f"[Enrollment] Registered {speaker_key} as {candidate}"
        )

        should_announce = True
        if self.enrollment_prompted and candidate not in self.completed_enrollments:
            should_announce = False

        if not self.enrollment_complete and should_announce:
            await self._announce_enrollment(candidate)
            if len(self.speaker_assignments) == len(self.player_names):
                self.enrollment_complete = True
                await self._announce_enrollment_completion()

        return candidate

    async def _announce_enrollment(self, player_name: str):
        confirmation = f"{player_name}'s voice is locked in."
        await self._send_transcript("assistant", confirmation, True, 100)
        await self._send_to_tts(confirmation, True)

    async def _announce_enrollment_completion(self):
        wrap_up = "All players are registered. Let's play Guess Who Likes What!"
        await self._send_transcript("assistant", wrap_up, True, 100)
        await self._send_to_tts(wrap_up, True)
        await self._start_food_round()

    async def _handle_unknown_speaker(self, text: str):
        """
        Notify the room when speech comes from an unregistered voice.
        """
        now = time.time()
        if now - self.last_unknown_speaker_ts < 5:
            self.ten_env.log_warn(
                "[MainControlExtension] Ignoring unknown speaker (rate limited)."
            )
            return

        self.last_unknown_speaker_ts = now
        message = (
            "I don't recognize that voice. Only Elliot, Taytay, and Musk are part of Who Likes What."
        )
        await self._send_transcript("assistant", message, True, 100)
        await self._send_to_tts(message, True)
        self.ten_env.log_warn(
            f"[MainControlExtension] Unrecognized speaker for text='{text}'"
        )

    async def _start_food_round(self):
        self.game_stage = "await_elliot_food"
        self.food_preferences = {}
        self.questions_answered = set()
        intro = (
            "Time for Guess Who Likes What! Elliot, Taytay, and Musk: we're guessing favorite foods."
        )
        await self._send_transcript("assistant", intro, True, 100)
        await self._send_to_tts(intro, True)
        await self._prompt_player_for_food("Elliot")

    async def _prompt_player_for_food(self, player_name: str):
        stage_map = {
            "Elliot": "await_elliot_food",
            "Musk": "await_musk_food",
            "Taytay": "await_taytay_food",
        }
        self.last_turn_reminder_ts.clear()
        if player_name in stage_map:
            self.game_stage = stage_map[player_name]
        prompt = f"{player_name}, tell me something you love to eat."
        await self._send_transcript("assistant", prompt, True, 100)
        await self._send_to_tts(prompt, True, player_name)

    async def _acknowledge_food(self, player_name: str, food_text: str):
        normalized_text = self._normalize_food_text(food_text)
        printable = normalized_text.rstrip(".!?") or food_text.strip()
        pronoun, verb = self._player_pronoun(player_name)
        summary = f"Got it, {player_name}! {pronoun.capitalize()} {verb} to eat {printable}."
        await self._send_transcript("assistant", summary, True, 100)
        await self._send_to_tts(summary, True, player_name)

    async def _remind_turn(self, expected_player: str, interrupting_player: str):
        now = time.time()
        last_reminder = self.last_turn_reminder_ts.get(interrupting_player, 0.0)
        if now - last_reminder < 4.0:
            self.ten_env.log_info(
                f"[MainControlExtension] Suppressing duplicate turn reminder for {interrupting_player}"
            )
            return
        self.last_turn_reminder_ts[interrupting_player] = now
        reminder = (
            f"Hang tight, {interrupting_player}. It's {expected_player}'s turn to share their food."
        )
        await self._send_transcript("assistant", reminder, True, 100)
        await self._send_to_tts(reminder, True, interrupting_player)

    async def _prompt_question_round(self):
        self.game_stage = "qa_phase"
        self.questions_answered = set()
        self.awaiting_additional_request = False
        self.last_turn_reminder_ts.clear()
        cue = (
            "Elliot, now quiz me! Ask what Musk likes to eat, then Taytay, and finally ask what you like to eat."
        )
        await self._send_transcript("assistant", cue, True, 100)
        await self._send_to_tts(cue, True, "Elliot")

    async def _respond_with_food(self, about_player: str, recipient: str):
        food_text = self.food_preferences.get(about_player)
        if not food_text:
            reply = f"I'm still waiting to hear what {about_player} loves to eat."
        else:
            pronoun, verb = self._player_pronoun(about_player)
            reply = f"{about_player} said {pronoun} {verb} to eat {food_text}."
        await self._send_transcript("assistant", reply, True, 100)
        await self._send_to_tts(reply, True, recipient)

    async def _prompt_anything_else(self):
        if self.awaiting_additional_request:
            return
        self.awaiting_additional_request = True
        self.game_stage = "await_additional_request"
        self.last_turn_reminder_ts.clear()
        question = "Anything else I can do?"
        await self._send_transcript("assistant", question, True, 100)
        await self._send_to_tts(question, True, "Elliot")

    @staticmethod
    def _is_shanghai_restaurant_request(normalized: str) -> bool:
        if "shanghai" not in normalized or "restaurant" not in normalized:
            return False
        if "food" not in normalized and "these" not in normalized:
            return False
        return True

    async def _respond_with_shanghai_restaurant(self, recipient: str):
        elliot_food = self.food_preferences.get("Elliot", "burger and fries")
        musk_food = self.food_preferences.get("Musk", "steak and seasoned rice")
        taytay_food = self.food_preferences.get("Taytay", "chocolate cookies and strawberry muffins")
        reply = (
            "Since you're all in Shanghai, you could visit The Bund Food Hall. "
            f"They serve {elliot_food} for Elliot, {musk_food} for Musk, and sweet treats like {taytay_food} for Taytay."
        )
        await self._send_transcript("assistant", reply, True, 100)
        await self._send_to_tts(reply, True, recipient)

    def _question_mentions_player(self, normalized: str, target: str) -> bool:
        """
        Returns True if the question text clearly references the target player (by name or alias).
        """
        aliases = {target.lower()}
        aliases.update(alias.lower() for alias in self.PLAYER_ALIAS_MAP.get(target, []))
        for alias in aliases:
            if re.search(rf"\b{re.escape(alias)}\b", normalized):
                return True
        return False

    def _is_follow_up_question_for(self, normalized: str, target: str) -> bool:
        """
        Detects short follow-up prompts like 'what about Taytay?' that rely on prior context.
        """
        aliases = {target.lower()}
        aliases.update(alias.lower() for alias in self.PLAYER_ALIAS_MAP.get(target, []))
        for alias in aliases:
            if any(
                re.search(pattern, normalized)
                for pattern in [
                    rf"\bwhat about {re.escape(alias)}\b",
                    rf"\bhow about {re.escape(alias)}\b",
                    rf"\band {re.escape(alias)}\b",
                    rf"\bwhat about the {re.escape(alias)}\b",
                ]
            ):
                return True
        return False

    def _is_food_question_for(self, normalized: str, target: str) -> bool:
        """
        Heuristic matching for questions asking about a target player's food preference.
        """
        if not self._question_mentions_player(normalized, target):
            return False
        follow_up = self._is_follow_up_question_for(normalized, target)

        question_markers = [
            "what",
            "tell me",
            "do you know",
            "could you tell",
            "can you tell",
            "remind me",
        ]
        has_question_marker = any(marker in normalized for marker in question_markers)
        if not has_question_marker and not follow_up:
            if "?" in normalized:
                implied_patterns = [
                    " like to eat",
                    " love to eat",
                    " like eating",
                    " love eating",
                    " prefer to eat",
                ]
                if any(pattern in normalized for pattern in implied_patterns):
                    has_question_marker = True
        if not has_question_marker and not follow_up:
            return False

        preference_markers = [
            "eat",
            "food",
            "favorite",
            "favourite",
            "like",
            "love",
            "enjoy",
        ]
        if not follow_up and not any(marker in normalized for marker in preference_markers):
            return False

        if not follow_up and "like" not in normalized and "love" not in normalized:
            if not any(marker in normalized for marker in ["eat", "food", "favorite", "favourite"]):
                return False

        return True

    @staticmethod
    def _is_self_food_question(normalized: str) -> bool:
        """
        Detects when Elliot asks about their own preference without naming themselves.
        """
        padded = f" {normalized} "
        if not any(
            phrase in padded
            for phrase in [
                " what do i ",
                " what i like",
                " what is my ",
                " what's my ",
                " what would i ",
                " remind me what i ",
                " tell me what i ",
            ]
        ):
            return False

        follow_up_phrases = [
            "what about me",
            "how about me",
            "what about myself",
            "how about myself",
            "and me",
        ]
        if any(phrase in normalized for phrase in follow_up_phrases):
            return True

        preference_markers = [
            "eat",
            "food",
            "favorite",
            "favourite",
            "like",
            "love",
            "enjoy",
        ]
        return any(marker in normalized for marker in preference_markers)

    async def _handle_game_flow(self, speaker: Optional[str], text: str) -> bool:
        if not speaker:
            return False
        clean_text = text.strip()
        if clean_text == "":
            return True

        stage = self.game_stage
        lower = clean_text.lower()

        if stage == "await_elliot_food":
            if speaker == "Elliot":
                self.food_preferences["Elliot"] = self._normalize_food_text(clean_text)
                await self._acknowledge_food("Elliot", clean_text)
                await self._prompt_player_for_food("Musk")
                return True
            if speaker in self.player_names:
                await self._remind_turn("Elliot", speaker)
                return True
            return False

        if stage == "await_musk_food":
            if speaker == "Musk":
                self.food_preferences["Musk"] = self._normalize_food_text(clean_text)
                await self._acknowledge_food("Musk", clean_text)
                await self._prompt_player_for_food("Taytay")
                return True
            if speaker in self.player_names:
                await self._remind_turn("Musk", speaker)
                return True
            return False

        if stage == "await_taytay_food":
            if speaker == "Taytay":
                self.food_preferences["Taytay"] = self._normalize_food_text(clean_text)
                await self._acknowledge_food("Taytay", clean_text)
                await self._prompt_question_round()
                return True
            if speaker in self.player_names:
                await self._remind_turn("Taytay", speaker)
                return True
            return False

        if stage == "qa_phase":
            if speaker != "Elliot":
                return False

            normalized = lower.replace("turmp", "taytay")

            handled = False
            if (
                "musk" not in self.questions_answered
                and self._is_food_question_for(normalized, "Musk")
            ):
                await self._respond_with_food("Musk", "Elliot")
                self.questions_answered.add("musk")
                handled = True
            elif (
                "taytay" not in self.questions_answered
                and self._is_food_question_for(normalized, "Taytay")
            ):
                await self._respond_with_food("Taytay", "Elliot")
                self.questions_answered.add("taytay")
                handled = True
            elif (
                "elliot" not in self.questions_answered
                and (
                    self._is_food_question_for(normalized, "Elliot")
                    or self._is_self_food_question(normalized)
                )
            ):
                await self._respond_with_food("Elliot", "Elliot")
                self.questions_answered.add("elliot")
                handled = True

            if handled and self.questions_answered.issuperset(
                {"musk", "taytay", "elliot"}
            ):
                await self._prompt_anything_else()
            return handled

        if stage == "await_additional_request":
            if speaker != "Elliot":
                return False
            normalized = lower
            if self._is_shanghai_restaurant_request(normalized):
                await self._respond_with_shanghai_restaurant("Elliot")
                self.awaiting_additional_request = False
                self.game_stage = "complete"
                return True
            return False

        return False

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        self.pending_response_target = None
        self.food_preferences = {}
        self.questions_answered = set()
        self.game_stage = "enrollment"
        self.speaker_assignments = {}
        self.enrollment_prompted = False
        self.enrollment_complete = False
        self.enrollment_index = 0
        self.completed_enrollments = set()
        self.awaiting_additional_request = False
        self.last_turn_reminder_ts = {}
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
