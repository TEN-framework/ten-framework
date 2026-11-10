"""The finite-input barrier between ASR, Agora Mind and TTS."""

from dataclasses import dataclass, field


@dataclass
class Completion:
    """Keep receipts, including turns that arrive before input acceptance."""

    finalize_id: str | None = None
    asr_finalized: bool = False
    pending_submissions: int = 0
    accepted_inputs: set[int] = field(default_factory=set)
    completed_ranges: list[tuple[int, int]] = field(default_factory=list)
    tts_request_ids: list[str] = field(default_factory=list)
    failed: bool = False
    reported: bool = False

    def ready(self) -> bool:
        return (
            self.finalize_id is not None
            and self.asr_finalized
            and self.pending_submissions == 0
            and not self.failed
            and not self.reported
            and all(
                any(
                    first <= sequence <= last
                    for first, last in self.completed_ranges
                )
                for sequence in self.accepted_inputs
            )
        )
