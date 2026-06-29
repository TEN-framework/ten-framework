# ABOUTME: Sentence-end boundary helpers for Soniox ASR finality gating.
# ABOUTME: Encapsulates terminator-run rules and pySBD probe for ambiguous periods.

from __future__ import annotations
import pysbd


class SentenceBoundaryDetector:
    """Detects sentence boundaries for zh/en Soniox finality gating."""

    TERMINATOR_CHARS = frozenset("。！？!?.…")
    CJK_TERMINATORS = frozenset("。！？")

    def __init__(self) -> None:
        self._segmenters: dict[str, pysbd.Segmenter] = {}

    def supports_language(self, language: str) -> bool:
        """Return True when sentence-end gating applies to this language."""
        return self._language_family(language) is not None

    def ends_with_boundary(self, text: str, language: str) -> bool:
        """Return True when stripped text ends at a sentence boundary."""
        stripped = text.rstrip()
        if not stripped:
            return False
        return self._boundary_at_tail(text, language, None)

    def split_at_last_complete_sentence(
        self,
        token_texts: list[str],
        language: str,
    ) -> tuple[int, int]:
        """Split token texts after the last complete sentence at token boundaries.

        Returns ``(emit_len, defer_start)`` indices into ``token_texts``:
        - ``emit_len == 0`` means nothing is ready to emit yet (defer all)
        - ``emit_len == len(token_texts)`` means the full sequence is one sentence
        - otherwise emit ``token_texts[:emit_len]`` and defer the remainder
        """
        if not token_texts or not self.supports_language(language):
            return 0, 0

        last_boundary = -1
        cumulative = ""
        for index, piece in enumerate(token_texts):
            cumulative += piece
            if len(cumulative.rstrip()) < len(cumulative):
                continue

            following_char: str | None = None
            if index + 1 < len(token_texts) and token_texts[index + 1]:
                following_char = token_texts[index + 1][0]

            if self._boundary_at_tail(cumulative, language, following_char):
                last_boundary = index

        if last_boundary < 0:
            return 0, 0

        remainder_start = last_boundary + 1
        if remainder_start >= len(token_texts):
            return len(token_texts), len(token_texts)

        return last_boundary + 1, remainder_start

    def _language_family(self, language: str) -> str | None:
        normalized = (language or "").lower().replace("_", "-")
        if normalized.startswith("zh") or normalized in ("cmn", "yue", "wuu"):
            return "zh"
        if normalized.startswith("en"):
            return "en"
        return None

    def _pysbd_language(self, language_family: str) -> str:
        return "zh" if language_family == "zh" else "en"

    def _get_segmenter(self, language_family: str) -> pysbd.Segmenter:
        pysbd_lang = self._pysbd_language(language_family)
        segmenter = self._segmenters.get(pysbd_lang)
        if segmenter is None:
            segmenter = pysbd.Segmenter(language=pysbd_lang, clean=False)
            self._segmenters[pysbd_lang] = segmenter
        return segmenter

    def _is_terminal_char(self, ch: str) -> bool:
        return ch in self.TERMINATOR_CHARS

    def _tail_terminator_run_length(self, text: str) -> int:
        stripped = text.rstrip()
        run_length = 0
        while run_length < len(stripped):
            if not self._is_terminal_char(stripped[-(run_length + 1)]):
                break
            run_length += 1
        return run_length

    def _digit_period_boundary_decision(
        self, text: str, following_char: str | None
    ) -> bool | None:
        """Return boundary decision for digit-period cases, or None if not applicable."""
        stripped = text.rstrip()
        if len(stripped) < 2 or not stripped.endswith("."):
            return None
        if not stripped[-2].isdigit():
            return None
        if following_char is not None and not following_char.isdigit():
            return True
        return False

    def _pysbd_probe_tail_boundary(
        self,
        text: str,
        language_family: str,
    ) -> bool:
        stripped = text.rstrip()
        if not stripped:
            return False
        segmenter = self._get_segmenter(language_family)
        # Add a synthetic next sentence so pySBD must decide whether the
        # current tail is a complete first sentence. If it splits before "Z.",
        # the single dot is treated as a sentence boundary; otherwise it is
        # likely an abbreviation such as "Mr.".
        probe = f"{stripped} Z."
        parts = segmenter.segment(probe)
        if not parts:
            return False
        return parts[0].rstrip() == stripped

    def _boundary_at_tail(
        self,
        text: str,
        language: str,
        following_char: str | None,
    ) -> bool:
        """True when text ends at a sentence boundary.

        ``following_char`` is the first character of the next token in the same
        merge buffer. When ``None``, the tail is treated as utterance-complete.
        """
        if not self.supports_language(language):
            return False

        stripped = text.rstrip()
        run_length = self._tail_terminator_run_length(stripped)
        if run_length == 0:
            return False

        tail = stripped[-run_length:]

        if following_char is not None and self._is_terminal_char(
            following_char
        ):
            return False

        if tail == ".":
            # Numeric periods need following-token context in the streaming
            # path: "12.Hello" is a sentence boundary after "12.", while "12.5"
            # is a decimal and must wait. If the next char is unknown ("12."),
            # stay conservative and defer.
            digit_period_decision = self._digit_period_boundary_decision(
                stripped,
                following_char,
            )
            if digit_period_decision is not None:
                return digit_period_decision
            # Remaining single-dot cases are language-specific ambiguity such
            # as "Mr." vs "Hello.", so use pySBD as an abbreviation-aware probe.
            language_family = self._language_family(language)
            if language_family is None:
                return False
            return self._pysbd_probe_tail_boundary(
                stripped,
                language_family,
            )

        return True
