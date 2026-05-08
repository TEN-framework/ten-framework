# ABOUTME: Maps Main POC dialog_messages to Volcengine request.corpus.context string
# ABOUTME: context_data is newest-first per Volcengine dialog_ctx specification
from __future__ import annotations

import json
from typing import Any

MAX_CONTEXT_ITEMS = 20


def _lang_is_zh(language: str) -> bool:
    return language.lower().startswith("zh")


def _line_for_message(role: str, content: str, lang_zh: bool) -> str:
    c = (content or "").strip()
    if not c:
        return ""
    if role == "user":
        return f"用户：{c}" if lang_zh else f"User: {c}"
    if role == "assistant":
        return f"助手：{c}" if lang_zh else f"Assistant: {c}"
    return ""


def build_volc_dialog_context_str(
    dialog_messages: list[Any],
    *,
    language: str = "zh-CN",
    max_items: int = MAX_CONTEXT_ITEMS,
) -> str | None:
    """Build corpus.context value: inner JSON as a single string.

    Returns None when there is no dialog context (caller should remove corpus.context).
    """
    if not dialog_messages:
        return None

    lang_zh = _lang_is_zh(language)
    lines_chrono: list[str] = []
    for m in dialog_messages:
        if not isinstance(m, dict):
            continue
        role = m.get("role")
        if role not in ("user", "assistant"):
            continue
        line = _line_for_message(
            str(role), str(m.get("content") or ""), lang_zh
        )
        if line:
            lines_chrono.append(line)

    if not lines_chrono:
        return None

    newest_first = list(reversed(lines_chrono))[:max_items]

    context_data = [{"text": line} for line in newest_first]

    if not context_data:
        return None

    inner = {
        "context_type": "dialog_ctx",
        "context_data": context_data,
    }
    return json.dumps(inner, ensure_ascii=False)
