#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Black-box Guarder regressions using a standalone TEN fixture extension."""

from .test_subtitle_alignment import run_subtitle_alignment
from .timing_fixture.addon import TIMING_FIXTURE_ADDON
from .timing_fixture.provider import TimingProvider


def run_scenario(scenario, monkeypatch, expected_error=None):
    with TimingProvider(scenario) as provider:
        error = run_subtitle_alignment(
            TIMING_FIXTURE_ADDON,
            {"scenario": scenario, "url": provider.url},
            monkeypatch,
            observe_provider=True,
        )
    if expected_error is None:
        assert error is None, error.error_message() if error else ""
    else:
        assert error is not None
        assert expected_error in error.error_message()


def test_guarder_rejects_cumulative_frame_drift(monkeypatch):
    run_scenario(
        "cumulative_drift", monkeypatch, "cumulative sample clock mismatch"
    )


def test_guarder_accepts_fractional_sample_clock(monkeypatch):
    run_scenario("fractional_clock", monkeypatch)


def test_guarder_allows_leading_silence_and_ignores_empty_terminal_time(
    monkeypatch,
):
    run_scenario("leading_silence", monkeypatch)


def test_guarder_rejects_words_beyond_actual_sample_duration(monkeypatch):
    run_scenario(
        "word_out_of_bounds", monkeypatch, "word lies outside the PCM timeline"
    )


def test_guarder_rejects_result_range_beyond_actual_sample_duration(
    monkeypatch,
):
    run_scenario(
        "result_out_of_bounds",
        monkeypatch,
        "range lies outside the PCM timeline",
    )


def test_guarder_rejects_first_word_offset_erasure(monkeypatch):
    run_scenario(
        "erased_offset", monkeypatch, "word timing differs from MiniMax"
    )


def test_guarder_compares_each_request_with_its_own_pcm_origin(monkeypatch):
    run_scenario("separate_origins", monkeypatch)
