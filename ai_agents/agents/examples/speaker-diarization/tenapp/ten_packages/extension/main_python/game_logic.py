from __future__ import annotations

import re
import string
import time
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:  # pragma: no cover - for type hints only
    from .extension import MainControlExtension


class WhoLikesWhatGame:
    """Encapsulates the \"Who Likes What\" game state and helper routines."""

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

    def __init__(self, extension: MainControlExtension):
        self.ext = extension
        self.player_names: list[str] = ["Elliot", "Musk", "Taytay"]
        self.reset_state()

    def reset_state(self) -> None:
        self.speaker_assignments: dict[str, str] = {}
        self.enrollment_prompted: bool = False
        self.enrollment_complete: bool = False
        self.enrollment_order: list[str] = list(self.player_names)
        self.enrollment_index: int = 0
        self.completed_enrollments: set[str] = set()
        self.last_speaker: str = ""
        self.last_unknown_speaker_ts: float = 0.0
        self.last_turn_reminder_ts: dict[str, float] = {}
        self.game_stage: str = "enrollment"
        self.food_preferences: dict[str, str] = {}
        self.questions_answered: set[str] = set()
        self.awaiting_additional_request: bool = False

    # ------------------------------------------------------------------
    # Generic helpers
    # ------------------------------------------------------------------
    @staticmethod
    def normalize_label(value) -> str:
        if value is None:
            return ""
        if isinstance(value, str):
            value = value.strip()
        if value == "":
            return ""
        return str(value).upper()

    @staticmethod
    def build_speaker_key(speaker: str, channel: str) -> str:
        if speaker:
            return f"speaker:{speaker}"
        if channel:
            return f"channel:{channel}"
        return ""

    def detect_declared_player(self, text: str) -> Optional[str]:
        if not text:
            return None
        lowered = text.lower()
        stripped = lowered.strip(string.whitespace + string.punctuation)
        for player, aliases in self.PLAYER_ALIAS_MAP.items():
            for alias in aliases:
                alias_lower = alias.lower()
                # Direct match such as "Elliot" or "Elliot."
                if stripped == alias_lower:
                    return player
                # Greeting phrases like "Hello Elliot"
                for template in self.GREETING_TEMPLATES:
                    if stripped == template.format(alias=alias_lower):
                        return player
                # Introduction statements
                escaped_alias = re.escape(alias_lower)
                for pattern in self.INTRODUCTION_PATTERNS:
                    if re.search(pattern.format(alias=escaped_alias), lowered):
                        return player
        return None

    @staticmethod
    def normalize_food_text(text: str) -> str:
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
            match = re.match(pattern, lowered)
            if match:
                span = match.span()
                working = working[span[1] :]
                break

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
                        trimmed = trimmed[len(prefix) :]
                        changed = True
                        break
            return trimmed

        working = _strip_prefix(working.lstrip())

        working = working.strip(string.whitespace + string.punctuation)
        return working or text.strip()

    def looks_like_reassignment(
        self,
        text: str,
        current_name: str,
        new_name: str,
        enrollment_active: bool,
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
            aliases = self.PLAYER_ALIAS_MAP.get(new_name, [new_name])
            for alias in aliases:
                alias_lower = alias.lower()
                if stripped == alias_lower:
                    return True
                for template in self.GREETING_TEMPLATES:
                    if stripped == template.format(alias=alias_lower):
                        return True
                escaped_alias = re.escape(alias_lower)
                for pattern in self.INTRODUCTION_PATTERNS:
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

    # ------------------------------------------------------------------
    # Enrollment and diarization helpers
    # ------------------------------------------------------------------
    async def start_enrollment_flow(self) -> None:
        if self.enrollment_prompted:
            return
        self.enrollment_prompted = True
        self.enrollment_complete = False
        self.enrollment_index = 0
        self.completed_enrollments.clear()
        self.speaker_assignments.clear()
        self.last_turn_reminder_ts.clear()
        await self.prompt_current_enrollment()

    async def prompt_current_enrollment(self) -> None:
        if self.enrollment_index >= len(self.enrollment_order):
            return
        player_name = self.enrollment_order[self.enrollment_index]
        self.game_stage = f"enrollment_{player_name.lower()}"
        prompt = f"{player_name}, please say hello so I can learn your voice."
        await self.ext._send_transcript("assistant", prompt, True, 100)
        await self.ext._send_to_tts(prompt, True, player_name)

    async def handle_enrollment_stage(
        self,
        speaker: Optional[str],
        speaker_key: str,
        transcript_text: str,
    ) -> None:
        if self.enrollment_index >= len(self.enrollment_order):
            return

        expected_player = self.enrollment_order[self.enrollment_index]
        if speaker != expected_player:
            self.ext.ten_env.log_info(
                f"[Enrollment] Received speech from {speaker or 'unknown'} while awaiting {expected_player}"
            )
            return

        if expected_player in self.completed_enrollments:
            return

        self.completed_enrollments.add(expected_player)
        await self.announce_enrollment(expected_player)
        self.enrollment_index += 1

        if self.enrollment_index < len(self.enrollment_order):
            await self.prompt_current_enrollment()
        else:
            self.enrollment_complete = True
            await self.announce_enrollment_completion()

    async def assign_player_if_needed(
        self,
        speaker_key: str,
        transcript_text: str = "",
        allow_reassignment: bool = True,
    ) -> Optional[str]:
        if not speaker_key:
            return None
        declared = (
            self.detect_declared_player(transcript_text)
            if allow_reassignment
            else None
        )

        existing = self.speaker_assignments.get(speaker_key)
        if existing:
            if allow_reassignment and declared and declared != existing:
                if not self.looks_like_reassignment(
                    transcript_text,
                    existing,
                    declared,
                    not self.enrollment_complete,
                ):
                    self.ext.ten_env.log_info(
                        f"[Enrollment] Ignoring mention of {declared} from already registered {existing}"
                    )
                    return existing
                for key, value in list(self.speaker_assignments.items()):
                    if key != speaker_key and value == declared:
                        del self.speaker_assignments[key]
                        break
                self.speaker_assignments[speaker_key] = declared
                self.ext.ten_env.log_info(
                    f"[Enrollment] Corrected {speaker_key} -> {declared}"
                )
                if not self.enrollment_complete:
                    await self.announce_enrollment(declared)
                    if len(self.speaker_assignments) == len(self.player_names):
                        self.enrollment_complete = True
                        await self.announce_enrollment_completion()
                return declared
            return existing

        if len(self.speaker_assignments) >= len(self.player_names):
            return None

        candidate: Optional[str] = None
        if (
            allow_reassignment
            and declared
            and declared not in self.speaker_assignments.values()
        ):
            candidate = declared
        else:
            for name in self.player_names:
                if name not in self.speaker_assignments.values():
                    candidate = name
                    break

        if not candidate:
            return None

        self.speaker_assignments[speaker_key] = candidate
        self.ext.ten_env.log_info(
            f"[Enrollment] Registered {speaker_key} as {candidate}"
        )

        should_announce = True
        if (
            self.enrollment_prompted
            and candidate not in self.completed_enrollments
        ):
            should_announce = False

        if not self.enrollment_complete and should_announce:
            await self.announce_enrollment(candidate)
            if len(self.speaker_assignments) == len(self.player_names):
                self.enrollment_complete = True
                await self.announce_enrollment_completion()

        return candidate

    async def announce_enrollment(self, player_name: str) -> None:
        confirmation = f"{player_name}'s voice is locked in."
        await self.ext._send_transcript("assistant", confirmation, True, 100)
        await self.ext._send_to_tts(confirmation, True)

    async def announce_enrollment_completion(self) -> None:
        wrap_up = "All players are registered. Let's play Guess Who Likes What!"
        await self.ext._send_transcript("assistant", wrap_up, True, 100)
        await self.ext._send_to_tts(wrap_up, True)
        await self.start_food_round()

    async def handle_unknown_speaker(self, text: str) -> None:
        now = time.time()
        if now - self.last_unknown_speaker_ts < 5:
            self.ext.ten_env.log_warn(
                "[MainControlExtension] Ignoring unknown speaker (rate limited)."
            )
            return

        self.last_unknown_speaker_ts = now
        message = "I don't recognize that voice. Only Elliot, Taytay, and Musk are part of Who Likes What."
        await self.ext._send_transcript("assistant", message, True, 100)
        await self.ext._send_to_tts(message, True)
        self.ext.ten_env.log_warn(
            f"[MainControlExtension] Unrecognized speaker for text='{text}'"
        )

    # ------------------------------------------------------------------
    # Game flow helpers
    # ------------------------------------------------------------------
    async def start_food_round(self) -> None:
        self.game_stage = "await_elliot_food"
        self.food_preferences = {}
        self.questions_answered = set()
        intro = "Time for Guess Who Likes What! Elliot, Taytay, and Musk: we're guessing favorite foods."
        await self.ext._send_transcript("assistant", intro, True, 100)
        await self.ext._send_to_tts(intro, True)
        await self.prompt_player_for_food("Elliot")

    async def prompt_player_for_food(self, player_name: str) -> None:
        stage_map = {
            "Elliot": "await_elliot_food",
            "Musk": "await_musk_food",
            "Taytay": "await_taytay_food",
        }
        self.last_turn_reminder_ts.clear()
        if player_name in stage_map:
            self.game_stage = stage_map[player_name]
        prompt = f"{player_name}, tell me something you love to eat."
        await self.ext._send_transcript("assistant", prompt, True, 100)
        await self.ext._send_to_tts(prompt, True, player_name)

    async def acknowledge_food(self, player_name: str, food_text: str) -> None:
        normalized_text = self.normalize_food_text(food_text)
        printable = normalized_text.rstrip(".!?") or food_text.strip()
        pronoun, verb = self.player_pronoun(player_name)
        summary = f"Got it, {player_name}! {pronoun.capitalize()} {verb} to eat {printable}."
        await self.ext._send_transcript("assistant", summary, True, 100)
        await self.ext._send_to_tts(summary, True, player_name)

    async def remind_turn(
        self, expected_player: str, interrupting_player: str
    ) -> None:
        now = time.time()
        last_reminder = self.last_turn_reminder_ts.get(interrupting_player, 0.0)
        if now - last_reminder < 4.0:
            self.ext.ten_env.log_info(
                f"[MainControlExtension] Suppressing duplicate turn reminder for {interrupting_player}"
            )
            return
        self.last_turn_reminder_ts[interrupting_player] = now
        reminder = f"Hang tight, {interrupting_player}. It's {expected_player}'s turn to share their food."
        await self.ext._send_transcript("assistant", reminder, True, 100)
        await self.ext._send_to_tts(reminder, True, interrupting_player)

    async def prompt_question_round(self) -> None:
        self.game_stage = "qa_phase"
        self.questions_answered = set()
        self.awaiting_additional_request = False
        self.last_turn_reminder_ts.clear()
        cue = "Elliot, now quiz me! Ask what Musk likes to eat, then Taytay, and finally ask what you like to eat."
        await self.ext._send_transcript("assistant", cue, True, 100)
        await self.ext._send_to_tts(cue, True, "Elliot")

    async def respond_with_food(
        self, about_player: str, recipient: str
    ) -> None:
        food_text = self.food_preferences.get(about_player)
        if not food_text:
            reply = (
                f"I'm still waiting to hear what {about_player} loves to eat."
            )
        else:
            pronoun, verb = self.player_pronoun(about_player)
            reply = f"{about_player} said {pronoun} {verb} to eat {food_text}."
        await self.ext._send_transcript("assistant", reply, True, 100)
        await self.ext._send_to_tts(reply, True, recipient)

    async def prompt_anything_else(self) -> None:
        if self.awaiting_additional_request:
            return
        self.awaiting_additional_request = True
        self.game_stage = "await_additional_request"
        self.last_turn_reminder_ts.clear()
        question = "Anything else I can do?"
        await self.ext._send_transcript("assistant", question, True, 100)
        await self.ext._send_to_tts(question, True, "Elliot")

    @staticmethod
    def is_shanghai_restaurant_request(normalized: str) -> bool:
        if "shanghai" not in normalized or "restaurant" not in normalized:
            return False
        if "food" not in normalized and "these" not in normalized:
            return False
        return True

    async def respond_with_shanghai_restaurant(self, recipient: str) -> None:
        elliot_food = self.food_preferences.get("Elliot", "burger and fries")
        musk_food = self.food_preferences.get("Musk", "steak and seasoned rice")
        taytay_food = self.food_preferences.get(
            "Taytay", "chocolate cookies and strawberry muffins"
        )
        reply = (
            "Since you're all in Shanghai, you could visit The Bund Food Hall. "
            f"They serve {elliot_food} for Elliot, {musk_food} for Musk, and sweet treats like {taytay_food} for Taytay."
        )
        await self.ext._send_transcript("assistant", reply, True, 100)
        await self.ext._send_to_tts(reply, True, recipient)

    def question_mentions_player(self, normalized: str, target: str) -> bool:
        aliases = {target.lower()}
        aliases.update(
            alias.lower() for alias in self.PLAYER_ALIAS_MAP.get(target, [])
        )
        for alias in aliases:
            if re.search(rf"\b{re.escape(alias)}\b", normalized):
                return True
        return False

    def is_follow_up_question_for(self, normalized: str, target: str) -> bool:
        aliases = {target.lower()}
        aliases.update(
            alias.lower() for alias in self.PLAYER_ALIAS_MAP.get(target, [])
        )
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

    def is_food_question_for(self, normalized: str, target: str) -> bool:
        if not self.question_mentions_player(normalized, target):
            return False
        follow_up = self.is_follow_up_question_for(normalized, target)

        question_markers = [
            "what",
            "tell me",
            "do you know",
            "could you tell",
            "can you tell",
            "remind me",
        ]
        has_question_marker = any(
            marker in normalized for marker in question_markers
        )
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
        if not follow_up and not any(
            marker in normalized for marker in preference_markers
        ):
            return False

        if (
            not follow_up
            and "like" not in normalized
            and "love" not in normalized
        ):
            if not any(
                marker in normalized
                for marker in ["eat", "food", "favorite", "favourite"]
            ):
                return False

        return True

    @staticmethod
    def is_self_food_question(normalized: str) -> bool:
        if "i" not in normalized:
            return False
        if "eat" not in normalized and "food" not in normalized:
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
        return any(marker in normalized for marker in preference_markers)

    def player_pronoun(self, player_name: str) -> tuple[str, str]:
        pronoun_map = {
            "Elliot": ("he", "loves"),
            "Musk": ("he", "loves"),
            "Taytay": ("she", "loves"),
        }
        return pronoun_map.get(player_name, ("they", "love"))

    async def handle_game_flow(self, speaker: Optional[str], text: str) -> bool:
        if not speaker:
            return False
        clean_text = text.strip()
        if clean_text == "":
            return True

        stage = self.game_stage
        lower = clean_text.lower()

        if stage == "await_elliot_food":
            if speaker == "Elliot":
                self.food_preferences["Elliot"] = self.normalize_food_text(
                    clean_text
                )
                await self.acknowledge_food("Elliot", clean_text)
                await self.prompt_player_for_food("Musk")
                return True
            if speaker in self.player_names:
                await self.remind_turn("Elliot", speaker)
                return True
            return False

        if stage == "await_musk_food":
            if speaker == "Musk":
                self.food_preferences["Musk"] = self.normalize_food_text(
                    clean_text
                )
                await self.acknowledge_food("Musk", clean_text)
                await self.prompt_player_for_food("Taytay")
                return True
            if speaker in self.player_names:
                await self.remind_turn("Musk", speaker)
                return True
            return False

        if stage == "await_taytay_food":
            if speaker == "Taytay":
                self.food_preferences["Taytay"] = self.normalize_food_text(
                    clean_text
                )
                await self.acknowledge_food("Taytay", clean_text)
                await self.prompt_question_round()
                return True
            if speaker in self.player_names:
                await self.remind_turn("Taytay", speaker)
                return True
            return False

        if stage == "qa_phase":
            if speaker != "Elliot":
                return False

            normalized = lower.replace("turmp", "taytay")

            handled = False
            if (
                "musk" not in self.questions_answered
                and self.is_food_question_for(normalized, "Musk")
            ):
                await self.respond_with_food("Musk", "Elliot")
                self.questions_answered.add("musk")
                handled = True
            elif (
                "taytay" not in self.questions_answered
                and self.is_food_question_for(normalized, "Taytay")
            ):
                await self.respond_with_food("Taytay", "Elliot")
                self.questions_answered.add("taytay")
                handled = True
            elif "elliot" not in self.questions_answered and (
                self.is_food_question_for(normalized, "Elliot")
                or self.is_self_food_question(normalized)
            ):
                await self.respond_with_food("Elliot", "Elliot")
                self.questions_answered.add("elliot")
                handled = True

            if handled and self.questions_answered.issuperset(
                {"musk", "taytay", "elliot"}
            ):
                await self.prompt_anything_else()
            return handled

        if stage == "await_additional_request":
            if speaker != "Elliot":
                return False
            normalized = lower
            if self.is_shanghai_restaurant_request(normalized):
                await self.respond_with_shanghai_restaurant("Elliot")
                self.awaiting_additional_request = False
                self.game_stage = "complete"
                return True
            return False

        return False
