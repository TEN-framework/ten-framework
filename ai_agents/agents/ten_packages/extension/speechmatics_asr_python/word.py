#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from dataclasses import dataclass
from typing import List
from .config import SpeechmaticsASRConfig
from .language_utils import is_space_separated_language


@dataclass
class SpeechmaticsASRWord:
    word: str = ""
    start_ms: int = 0
    duration_ms: int = 0
    is_punctuation: bool = False
    speaker: str | None = None
    channel: str | None = None


def convert_words_to_sentence(
    words: List[SpeechmaticsASRWord], config: SpeechmaticsASRConfig
) -> str:
    if is_space_separated_language(config.language):
        return " ".join([word.word for word in words])
    else:
        return "".join([word.word for word in words])


def get_sentence_start_ms(words: List[SpeechmaticsASRWord]) -> int:
    return words[0].start_ms


def get_sentence_duration_ms(words: List[SpeechmaticsASRWord]) -> int:
    return sum([word.duration_ms for word in words])


def _append_token(
    existing: str,
    token: str,
    is_punctuation: bool,
    language: str,
) -> str:
    if not token:
        return existing

    if is_punctuation:
        return f"{existing}{token}"

    if existing and is_space_separated_language(language):
        return f"{existing} {token}"

    return f"{existing}{token}"


def get_speaker_segments(
    words: List[SpeechmaticsASRWord], config: SpeechmaticsASRConfig
) -> List[dict]:
    """Group contiguous words by speaker for metadata."""

    segments: List[dict] = []
    if not words:
        return segments

    current_segment: dict | None = None

    for word in words:
        speaker = word.speaker or "UU"
        channel = word.channel
        start_ms = word.start_ms
        end_ms = word.start_ms + word.duration_ms

        if current_segment and (
            current_segment["speaker"] == speaker
            and current_segment.get("channel") == channel
        ):
            current_segment["text"] = _append_token(
                current_segment["text"], word.word, word.is_punctuation, config.language
            )
            current_segment["end_ms"] = end_ms
            current_segment["duration_ms"] = max(
                0, end_ms - current_segment["start_ms"]
            )
            current_segment["words"].append(
                {
                    "word": word.word,
                    "start_ms": word.start_ms,
                    "duration_ms": word.duration_ms,
                    "is_punctuation": word.is_punctuation,
                }
            )
        else:
            current_segment = {
                "speaker": speaker,
                "channel": channel,
                "text": word.word,
                "start_ms": start_ms,
                "end_ms": end_ms,
                "duration_ms": max(0, word.duration_ms),
                "words": [
                    {
                        "word": word.word,
                        "start_ms": word.start_ms,
                        "duration_ms": word.duration_ms,
                        "is_punctuation": word.is_punctuation,
                    }
                ],
            }
            segments.append(current_segment)

    # Normalize duration for punctuation-only segments
    for segment in segments:
        segment["duration_ms"] = max(
            0, segment["end_ms"] - segment["start_ms"]
        )

    return segments
