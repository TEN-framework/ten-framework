#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import importlib.util
from pathlib import Path

# Load helper and openai modules directly from source files to avoid importing
# the extension package __init__ (which requires ten_runtime).
_ext_dir = Path(__file__).resolve().parents[1]


def _load(filename):
    path = _ext_dir / filename
    spec = importlib.util.spec_from_file_location(path.stem, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


_helper = _load("helper.py")
parse_sentences = _helper.parse_sentences
is_punctuation = _helper.is_punctuation


# ============================================================
# is_punctuation
# ============================================================


def test_is_punctuation_english():
    for ch in [",", ".", "?", "!"]:
        assert is_punctuation(ch), f"Expected '{ch}' to be punctuation"


def test_is_punctuation_chinese():
    for ch in ["，", "。", "？", "！"]:
        assert is_punctuation(ch), f"Expected '{ch}' to be punctuation"


def test_is_punctuation_letter_is_false():
    assert not is_punctuation("a")
    assert not is_punctuation("Z")
    assert not is_punctuation("1")
    assert not is_punctuation(" ")


# ============================================================
# parse_sentences
# ============================================================


def test_parse_sentences_single_sentence():
    sentences, remain = parse_sentences("", "Hello world.")
    assert sentences == ["Hello world."]
    assert remain == ""


def test_parse_sentences_multiple_sentences():
    sentences, remain = parse_sentences("", "First. Second. Third.")
    assert len(sentences) == 3


def test_parse_sentences_incomplete_sentence_remains():
    sentences, remain = parse_sentences("", "Hello world")
    assert sentences == []
    assert remain == "Hello world"


def test_parse_sentences_fragment_prepended():
    sentences, remain = parse_sentences("Hello", " world.")
    assert sentences == ["Hello world."]
    assert remain == ""


def test_parse_sentences_fragment_with_no_punctuation():
    sentences, remain = parse_sentences("Part one", " and more")
    assert sentences == []
    assert remain == "Part one and more"


def test_parse_sentences_chinese_punctuation():
    sentences, remain = parse_sentences("", "你好。再见。")
    assert len(sentences) == 2
    assert remain == ""


def test_parse_sentences_mixed_punctuation():
    sentences, remain = parse_sentences("", "Hello! How are you? Fine.")
    assert len(sentences) == 3


def test_parse_sentences_punctuation_only_not_emitted():
    """A token that is only punctuation (no alphanumeric) should be skipped."""
    sentences, remain = parse_sentences("", "...")
    assert sentences == []


def test_parse_sentences_empty_content():
    sentences, remain = parse_sentences("", "")
    assert sentences == []
    assert remain == ""


def test_parse_sentences_empty_fragment_and_content():
    sentences, remain = parse_sentences("", "")
    assert sentences == []
    assert remain == ""


def test_parse_sentences_question_mark():
    sentences, remain = parse_sentences("", "Are you there?")
    assert sentences == ["Are you there?"]
    assert remain == ""


def test_parse_sentences_exclamation():
    sentences, remain = parse_sentences("", "Watch out!")
    assert sentences == ["Watch out!"]
    assert remain == ""


# ============================================================
# ThinkParser (grok's simpler token-level parser)
# ============================================================


def _load_think_parser():
    # grok_python/openai.py contains ThinkParser but also imports openai and
    # requests at module level. Use importlib and patch those imports first.
    import sys
    import types
    from unittest.mock import MagicMock

    for mod_name in [
        "openai",
        "openai.types",
        "openai.types.chat",
        "openai.types.chat.chat_completion",
        "requests",
    ]:
        if mod_name not in sys.modules:
            m = types.ModuleType(mod_name)
            m.__getattr__ = lambda attr: MagicMock()
            sys.modules[mod_name] = m

    path = _ext_dir / "openai.py"
    spec = importlib.util.spec_from_file_location("grok_openai", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.ThinkParser


ThinkParser = _load_think_parser()


def test_think_parser_initial_state_normal():
    parser = ThinkParser()
    assert parser.state == "NORMAL"


def test_think_parser_open_tag_changes_state():
    parser = ThinkParser()
    changed = parser.process("<think>")
    assert changed is True
    assert parser.state == "THINK"


def test_think_parser_close_tag_changes_state():
    parser = ThinkParser()
    parser.process("<think>")
    changed = parser.process("</think>")
    assert changed is True
    assert parser.state == "NORMAL"


def test_think_parser_plain_text_no_state_change():
    parser = ThinkParser()
    changed = parser.process("hello")
    assert changed is False
    assert parser.state == "NORMAL"


def test_think_parser_accumulates_think_content():
    parser = ThinkParser()
    parser.process("<think>")
    parser.process("step one")
    parser.process("step two")
    assert "step one" in parser.think_content
    assert "step two" in parser.think_content


def test_think_parser_no_accumulation_in_normal_state():
    parser = ThinkParser()
    parser.process("visible text")
    assert parser.think_content == ""


def test_think_parser_process_by_reasoning_content_opens():
    parser = ThinkParser()
    changed = parser.process_by_reasoning_content("thinking...")
    assert changed is True
    assert parser.state == "THINK"
    assert "thinking..." in parser.think_content


def test_think_parser_process_by_reasoning_content_closes_on_empty():
    parser = ThinkParser()
    parser.process_by_reasoning_content("data")
    changed = parser.process_by_reasoning_content("")
    assert changed is True
    assert parser.state == "NORMAL"


def test_think_parser_process_by_reasoning_already_normal_no_change():
    parser = ThinkParser()
    changed = parser.process_by_reasoning_content("")
    assert changed is False
