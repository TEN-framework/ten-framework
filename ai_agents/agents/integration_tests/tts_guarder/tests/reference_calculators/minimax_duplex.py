#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Convert untouched MiniMax subtitles into request-relative word times."""

from fractions import Fraction

from ..timing_hooks.base import ReferenceWord, RequestTimingReference


def merge_words(subtitle):
    words = []
    for item in subtitle["timestamped_words"]:
        if (
            words
            and item.get("word_begin") is not None
            and item.get("word_end") is not None
            and (item["word_begin"], item["word_end"])
            == (words[-1].get("word_begin"), words[-1].get("word_end"))
        ):
            words[-1]["time_end"] = item["time_end"]
        else:
            words.append(dict(item))
    return words


def build_reference(capture):
    setting = capture.audio_setting
    sample_rate = setting.get("sample_rate", 0)
    channels = setting.get("channels", setting.get("channel", 1))
    error = capture.error
    captions = []
    try:
        if setting.get("format", "pcm") != "pcm":
            error = "MiniMax timing reference requires PCM audio"
        elif sample_rate > 0 and channels > 0 and not error:
            for subtitle in capture.subtitles:
                sentence_ms = Fraction(
                    subtitle.sentence_start_bytes * 1000,
                    2 * channels * sample_rate,
                )
                captions.append(
                    tuple(
                        ReferenceWord(
                            " " if item["word"] == "[SPACE]" else item["word"],
                            sentence_ms + Fraction(str(item["time_begin"])),
                            sentence_ms + Fraction(str(item["time_end"])),
                        )
                        for item in merge_words(subtitle.payload)
                    )
                )
    except (
        ArithmeticError,
        AttributeError,
        KeyError,
        TypeError,
        ValueError,
    ) as exc:
        error = "MiniMax reference calculation failed: " + type(exc).__name__
    return RequestTimingReference(
        "MiniMax",
        sample_rate,
        channels,
        capture.pcm_bytes,
        tuple(captions),
        error,
    )
