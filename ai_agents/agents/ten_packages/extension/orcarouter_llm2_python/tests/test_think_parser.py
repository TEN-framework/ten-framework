#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the streaming <think>...</think> reasoning parser.

These are pure-logic tests with no TEN runtime dependency, so they run in
any environment.
"""
import sys
from pathlib import Path

# The extension package dir (parent of tests/) holds think_parser.py.
_EXT_DIR = str(Path(__file__).resolve().parents[1])
if _EXT_DIR not in sys.path:
    sys.path.insert(0, _EXT_DIR)

from think_parser import ThinkParser  # noqa: E402


def _collect(parser: ThinkParser, chunks):
    """Feed content chunks through the parser and return the flat event list
    plus finalize() events."""
    events = []
    for chunk in chunks:
        events.extend(parser.process_content(chunk))
    events.extend(parser.finalize())
    return events


def _join(events, event_type):
    return "".join(v for t, v in events if t == event_type)


def test_plain_content_no_think():
    events = _collect(ThinkParser(), ["Hello ", "world"])
    assert _join(events, "message_delta") == "Hello world"
    assert not any(t.startswith("reasoning") for t, _ in events)


def test_single_think_block():
    events = _collect(
        ThinkParser(), ["before <think>reasoning here</think> after"]
    )
    assert _join(events, "message_delta") == "before  after"
    assert _join(events, "reasoning_delta") == "reasoning here"
    assert any(t == "reasoning_done" for t, _ in events)


def test_think_split_across_chunks():
    # The open/close tags are split across delta boundaries.
    events = _collect(
        ThinkParser(),
        ["ans:<thi", "nk>hidden ", "thought</thi", "nk>done"],
    )
    assert _join(events, "message_delta") == "ans:done"
    assert _join(events, "reasoning_delta") == "hidden thought"
    assert any(t == "reasoning_done" for t, _ in events)


def test_unclosed_think_finalized():
    # An open <think> with no closing tag should still flush on finalize().
    events = _collect(ThinkParser(), ["text <think>still thinking"])
    assert _join(events, "message_delta") == "text "
    assert _join(events, "reasoning_delta") == "still thinking"
    assert any(t == "reasoning_done" for t, _ in events)


def test_reasoning_content_channel():
    # Providers that stream a dedicated reasoning_content field.
    parser = ThinkParser()
    events = []
    events.extend(parser.process_reasoning_content("step 1 "))
    events.extend(parser.process_reasoning_content("step 2"))
    events.extend(parser.process_reasoning_content(""))  # signals done
    assert _join(events, "reasoning_delta") == "step 1 step 2"
    assert any(t == "reasoning_done" for t, _ in events)
