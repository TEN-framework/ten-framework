#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import importlib.util
import sys
from pathlib import Path

# Load think_parser directly from its source file to avoid importing the
# extension package __init__ (which requires ten_runtime).
_think_parser_path = Path(__file__).resolve().parents[1] / "think_parser.py"
_spec = importlib.util.spec_from_file_location("think_parser", _think_parser_path)
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
ThinkParser = _mod.ThinkParser


# ============================================================
# ThinkParser.process_content
# ============================================================


def test_plain_text_produces_message_delta():
    parser = ThinkParser()
    events = parser.process_content("Hello world")
    assert events == [("message_delta", "Hello world")]


def test_think_block_produces_reasoning_events():
    parser = ThinkParser()
    events = parser.process_content("<think>reason</think>answer")
    types = [e[0] for e in events]
    assert "reasoning_delta" in types
    assert "reasoning_done" in types
    assert "message_delta" in types


def test_think_block_reasoning_content():
    parser = ThinkParser()
    events = parser.process_content("<think>step one</think>")
    reasoning_deltas = [v for t, v in events if t == "reasoning_delta"]
    assert reasoning_deltas == ["step one"]
    reasoning_done = [v for t, v in events if t == "reasoning_done"]
    assert reasoning_done == ["step one"]


def test_message_after_think_block():
    parser = ThinkParser()
    events = parser.process_content("<think>r</think>answer")
    message_deltas = [v for t, v in events if t == "message_delta"]
    assert "answer" in message_deltas


def test_empty_input_returns_no_events():
    parser = ThinkParser()
    assert parser.process_content("") == []


def test_partial_open_tag_buffered():
    """Content ending with partial <think> prefix should be held in pending."""
    parser = ThinkParser()
    events = parser.process_content("Hello <th")
    # "Hello " is emitted; "<th" is pending (possible start of <think>)
    assert events == [("message_delta", "Hello ")]
    assert parser._pending == "<th"


def test_partial_open_tag_resolved_on_next_chunk():
    """A buffered partial open tag that turns out NOT to be <think> should flush."""
    parser = ThinkParser()
    parser.process_content("Hello <th")
    events = parser.process_content("e end")
    # "<the end" is not a think tag — should be emitted as message_delta
    combined = "".join(v for t, v in events if t == "message_delta")
    assert "<th" in combined or "e end" in combined


def test_split_think_tag_across_chunks():
    """<think> split across two chunks should still be detected."""
    parser = ThinkParser()
    parser.process_content("<thi")
    events = parser.process_content("nk>reason</think>")
    types = [e[0] for e in events]
    assert "reasoning_delta" in types


def test_state_returns_to_normal_after_think():
    parser = ThinkParser()
    parser.process_content("<think>r</think>")
    assert parser.state == "NORMAL"


def test_finalize_flushes_pending_normal():
    parser = ThinkParser()
    parser.process_content("hello <thi")
    events = parser.finalize()
    # pending "<thi" is flushed as message_delta (not a complete open tag)
    assert any(t == "message_delta" for t, _ in events)


def test_finalize_closes_open_think_block():
    parser = ThinkParser()
    parser.process_content("<think>unfinished")
    events = parser.finalize()
    types = [e[0] for e in events]
    assert "reasoning_done" in types
    assert parser.state == "NORMAL"


def test_multiple_think_blocks():
    parser = ThinkParser()
    events = parser.process_content("<think>a</think>mid<think>b</think>end")
    reasoning_done = [v for t, v in events if t == "reasoning_done"]
    assert len(reasoning_done) == 2


# ============================================================
# ThinkParser.process_reasoning_content
# ============================================================


def test_process_reasoning_content_emits_delta():
    parser = ThinkParser()
    events = parser.process_reasoning_content("thinking...")
    assert ("reasoning_delta", "thinking...") in events


def test_process_reasoning_content_empty_closes_block():
    parser = ThinkParser()
    parser.process_reasoning_content("step one")
    events = parser.process_reasoning_content("")
    assert any(t == "reasoning_done" for t, _ in events)
    assert parser.state == "NORMAL"


# ============================================================
# ThinkParser.process (legacy bool interface)
# ============================================================


def test_process_returns_false_for_plain_text():
    parser = ThinkParser()
    changed = parser.process("no tags here")
    assert changed is False


def test_process_returns_true_on_state_change():
    parser = ThinkParser()
    # Opening <think> transitions state NORMAL -> THINK
    changed = parser.process("<think>reasoning</think>")
    assert changed is True
