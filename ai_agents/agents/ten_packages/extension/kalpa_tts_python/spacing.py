#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""The space after a sentence, which Kalpa waits for before it speaks.

Kalpa starts speaking a sentence of an open utterance once whitespace
follows its closing mark, because text that ends in "Dr." or "3." may go
on. A TEN agent cuts the LLM's reply after each punctuation mark and gives
the space that follows to the next piece. A finished sentence would then
wait until the agent has cut the next piece as well.

`SentenceSpacing` adds that space when a piece ends a sentence and takes it
off the front of the next piece. Kalpa receives the same text, earlier.

One case cannot be told apart: a name written with a dot, such as
"example.com", when the agent cuts it at its first dot. It is sent as
"example. com".
"""

SENTENCE_MARKS = ".?!।"
# Another of these in the same word means the mark belongs to an address, an
# abbreviation or a number that may go on: "a@b.", "www.b.", "U.S.", "1.2.".
_INNER_MARKS = ".@/:"
_OPENERS = "([{“«"
_CLOSERS = ")]}”»"


class SentenceSpacing:
    """Follows the text of one request. Every piece goes through `apply`."""

    def __init__(self) -> None:
        # A space was added after the previous piece.
        self._added = False
        # The characters since the last whitespace.
        self._word = ""
        # Brackets and quotation marks that are still open.
        self._open = 0
        self._quoted = False

    def apply(self, text: str, last: bool) -> str:
        """Return the piece as it should be sent to Kalpa.

        `last` marks the piece that ends the request. It needs no space,
        since the request's flush follows it.
        """
        if not text:
            return text
        if self._added and text[0].isspace():
            text = text[1:]
        self._added = False
        for char in text:
            self._word = "" if char.isspace() else self._word + char
            if char == '"':
                self._quoted = not self._quoted
            elif char in _OPENERS:
                self._open += 1
            elif char in _CLOSERS and self._open:
                self._open -= 1
        if last or not self._ends_sentence():
            return text
        self._added = True
        self._word = ""
        return text + " "

    def _ends_sentence(self) -> bool:
        word = self._word
        if not word or word[-1] not in SENTENCE_MARKS:
            return False
        if self._open or self._quoted:
            # The closing mark may come first: `"I will."` is cut after the
            # full stop.
            return False
        body = word[:-1]
        if any(char in _INNER_MARKS for char in body):
            return False
        if word[-1] == ".":
            # "3." may become "3.14", and "U." may become "U.S.".
            return len(body) > 1 and not body[-1].isdigit()
        return bool(body)
