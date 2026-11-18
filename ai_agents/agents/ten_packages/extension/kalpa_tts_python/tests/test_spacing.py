#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import pytest

from ten_packages.extension.kalpa_tts_python.spacing import SentenceSpacing


def cut_like_ten(reply: str) -> list[str]:
    """Cut after each punctuation mark, as a TEN agent cuts an LLM reply.

    The space after a mark starts the next piece, and what is left when the
    reply ends goes out last.
    """
    pieces, current = [], ""
    for char in reply:
        current += char
        if char in ",.?!":
            pieces.append(current)
            current = ""
    return pieces + [current]


def send(pieces: list[str]) -> list[str]:
    spacing = SentenceSpacing()
    return [
        spacing.apply(piece, last=index == len(pieces) - 1)
        for index, piece in enumerate(pieces)
    ]


def test_a_finished_sentence_is_sent_with_the_space_that_follows_it():
    reply = "Sure, I can help with that. Is it for today? Great! One moment."
    pieces = cut_like_ten(reply)
    assert pieces[1] == " I can help with that."

    sent = send(pieces)

    assert sent == [
        "Sure,",
        " I can help with that. ",
        "Is it for today? ",
        "Great! ",
        "One moment. ",
        "",
    ]
    # Kalpa receives the reply as it was written.
    assert "".join(sent) == reply + " "


def test_the_piece_that_ends_the_request_gets_no_space():
    assert send(["One sentence."]) == ["One sentence."]
    assert send(["First.", " Second."]) == ["First. ", "Second."]


def test_only_one_leading_space_is_taken_from_the_next_piece():
    assert send(["Done.", "  Next,", ""]) == ["Done. ", " Next,", ""]
    assert send(["Done.", "\nNext,", ""]) == ["Done. ", "Next,", ""]
    # An empty piece in between does not cancel it.
    assert send(["Done.", "", " Next,", ""]) == ["Done. ", "", "Next,", ""]


def test_hindi_sentences_end_at_a_danda():
    assert send(["यह ठीक है।", " धन्यवाद।", ""]) == [
        "यह ठीक है। ",
        "धन्यवाद। ",
        "",
    ]


@pytest.mark.parametrize(
    "reply",
    [
        # A number, a version and a numbered item.
        "Pi is 3.14 and",
        "It runs on version 2.5 now",
        "1. Open the app",
        # Abbreviations written with single letters.
        "It ships to the U.S. and the U.K. from",
        "Fruit, e.g. apples",
        # Addresses.
        "Write to help@example.com or",
        "Open https://example.com/start?step=2 or",
        # A closing quotation mark or bracket comes after the full stop.
        'She said, "I will be there." Then',
        "It is closed (on Sundays.) Then",
        "It is closed “on Sundays.” Then",
        # Marks with no word before them.
        "Well ... maybe",
    ],
)
def test_text_that_may_go_on_after_a_mark_is_left_as_it_is(reply):
    pieces = cut_like_ten(reply)

    assert send(pieces) == pieces


def test_a_sentence_after_a_closed_quotation_gets_its_space():
    reply = 'She said, "I will be there." Then she left. '
    pieces = cut_like_ten(reply)

    sent = send(pieces)

    assert sent[-2] == '" Then she left. '
    assert "".join(sent) == reply
