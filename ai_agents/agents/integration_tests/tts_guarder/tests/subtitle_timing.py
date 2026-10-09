#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Independent checks for the PCM sample clock observed by the Guarder."""

import json
from copy import deepcopy
from dataclasses import dataclass


@dataclass(frozen=True)
class AudioFrameTiming:
    timestamp_ms: int
    samples: int
    sample_rate: int
    channels: int = 1

    @property
    def duration_ms(self) -> int:
        return (
            self.samples * 1000 // self.sample_rate
            if self.sample_rate > 0
            else 0
        )


def validate_audio_clock(frames: list[AudioFrameTiming]) -> tuple[bool, str]:
    if not frames:
        return False, "No audio frames received"
    first = frames[0]
    if first.sample_rate <= 0 or first.channels <= 0:
        return False, "Invalid audio format"
    samples_before = 0
    for index, current in enumerate(frames):
        if (
            current.sample_rate != first.sample_rate
            or current.channels != first.channels
        ):
            return False, "Audio format changed within the request"
        if current.samples <= 0:
            return False, "Audio frame contains no samples"
        expected = (
            first.timestamp_ms + samples_before * 1000 // first.sample_rate
        )
        if abs(current.timestamp_ms - expected) > 1:
            return (
                False,
                f"Audio cumulative sample clock mismatch at frame {index}: expected {expected}, got {current.timestamp_ms}",
            )
        samples_before += current.samples
    return True, "Audio timestamps follow cumulative PCM samples (within 1ms)"


def timed_results(results: list[dict]) -> list[dict]:
    return [result for result in results if result.get("words")]


def validate_subtitle_bounds(
    results: list[dict], frames: list[AudioFrameTiming]
) -> tuple[bool, str]:
    captions = timed_results(results)
    if not captions or not frames:
        return False, "Missing subtitle words or audio frames"
    first = frames[0]
    if first.sample_rate <= 0:
        return False, "Invalid audio sample rate"
    audio_end = (
        first.timestamp_ms
        + sum(frame.samples for frame in frames) * 1000 / first.sample_rate
    )
    previous_start = None
    for result in captions:
        start = result.get("start_ms", -1)
        duration = result.get("duration_ms", 0)
        if (
            duration < 0
            or start < first.timestamp_ms - 1
            or start > audio_end + 1
            or start + duration > audio_end + 1
        ):
            return False, "Subtitle range lies outside the PCM timeline"
        if previous_start is not None and start <= previous_start:
            return False, "Subtitle starts are not strictly ascending"
        previous_start = start
        previous_word = None
        for word in result["words"]:
            word_start = word.get("start_ms", -1)
            word_duration = word.get("duration_ms", -1)
            word_end = word_start + word_duration
            if (
                word_duration < 0
                or word_start < first.timestamp_ms - 1
                or word_end > audio_end + 1
            ):
                return False, "Subtitle word lies outside the PCM timeline"
            if word_start < start - 1 or (
                duration > 0 and word_end > start + duration + 1
            ):
                return False, "Subtitle word lies outside its result range"
            if previous_word is not None and word_start < previous_word:
                return False, "Subtitle words are not ascending"
            previous_word = word_start
    return True, "Subtitle words stay within PCM and their result ranges"


class MiniMaxTimingOracle:
    """Compare public TEN output with untouched provider messages at the socket."""

    def __init__(self, request_ids: list[str]):
        self.request_ids = request_ids
        self.request_index = 0
        self.sample_rate = 0
        self.channels = 1
        self.pcm_bytes = {request_id: 0 for request_id in request_ids}
        self.subtitles = {request_id: [] for request_id in request_ids}
        self.sentence_start_bytes = 0

    def observe_send(self, payload: str | bytes) -> None:
        message = json.loads(payload)
        if message.get("event") == "task_start":
            setting = message.get("audio_setting", {})
            self.sample_rate = setting.get("sample_rate", 0)
            self.channels = setting.get("channels", setting.get("channel", 1))

    def observe_receive(self, payload: str | bytes) -> None:
        message = json.loads(payload)
        if self.request_index >= len(self.request_ids):
            return
        request_id = self.request_ids[self.request_index]
        event = message.get("event")
        if event == "sentence_start":
            self.sentence_start_bytes = self.pcm_bytes[request_id]
        if event == "task_continued":
            data = message.get("data", {})
            self.pcm_bytes[request_id] += len(
                bytes.fromhex(data.get("audio") or "")
            )
            subtitle = data.get("subtitle")
            if subtitle and subtitle.get("timestamped_words"):
                self.subtitles[request_id].append(
                    (self.sentence_start_bytes, deepcopy(subtitle))
                )
        if event == "task_flushed":
            self.request_index += 1
            self.sentence_start_bytes = 0

    @staticmethod
    def expected_words(subtitle: dict) -> list[dict]:
        words = []
        for item in subtitle["timestamped_words"]:
            if (
                words
                and item.get("word_begin") is not None
                and item.get("word_end") is not None
                and (item.get("word_begin"), item.get("word_end"))
                == (words[-1].get("word_begin"), words[-1].get("word_end"))
            ):
                words[-1]["time_end"] = item["time_end"]
            else:
                words.append(dict(item))
        return words

    def validate(
        self,
        request_id: str,
        results: list[dict],
        frames: list[AudioFrameTiming],
    ) -> tuple[bool, str]:
        if any(result.get("request_id") != request_id for result in results):
            return False, "Subtitle belongs to another request"
        if self.sample_rate <= 0 or self.channels <= 0:
            return False, "MiniMax task_start audio format was not observed"
        if not frames or any(
            frame.sample_rate != self.sample_rate
            or frame.channels != self.channels
            for frame in frames
        ):
            return False, "TEN audio format differs from MiniMax task_start"
        observed_bytes = sum(
            frame.samples * frame.channels * 2 for frame in frames
        )
        if observed_bytes != self.pcm_bytes[request_id]:
            return False, "TEN PCM sample count differs from provider audio"
        captions = timed_results(results)
        expected = self.subtitles[request_id]
        if not expected or len(captions) != len(expected):
            return False, "TEN subtitle count differs from provider subtitles"
        origin = frames[0].timestamp_ms
        for result, (sentence_bytes, subtitle) in zip(captions, expected):
            words = self.expected_words(subtitle)
            if len(result["words"]) != len(words):
                return False, "TEN word count differs from provider words"
            sentence_ms = (
                sentence_bytes * 1000 / (2 * self.channels * self.sample_rate)
            )
            for actual, raw in zip(result["words"], words):
                expected_start = origin + int(sentence_ms + raw["time_begin"])
                expected_end = origin + int(sentence_ms + raw["time_end"])
                actual_end = actual["start_ms"] + actual["duration_ms"]
                expected_text = " " if raw["word"] == "[SPACE]" else raw["word"]
                if (
                    actual["word"] != expected_text
                    or abs(actual["start_ms"] - expected_start) > 1
                    or abs(actual_end - expected_end) > 1
                ):
                    return (
                        False,
                        f"TEN word timing differs from MiniMax: expected [{expected_start}, {expected_end}], got [{actual['start_ms']}, {actual_end}]",
                    )
        first_offset = expected[0][1]["timestamped_words"][0]["time_begin"]
        observed_offset = captions[0]["words"][0]["start_ms"] - origin
        return (
            True,
            f"MiniMax raw timing verified: {len(captions)} subtitles, first word offset {first_offset:.3f}ms -> {observed_offset}ms",
        )
