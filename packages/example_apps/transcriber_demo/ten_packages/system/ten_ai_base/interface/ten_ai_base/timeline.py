#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from enum import Enum
from typing import Callable, Optional


class AudioTimelineEventType(Enum):
    USER_AUDIO = 0
    SILENCE_AUDIO = 1
    DROPPED_AUDIO = 2  # Audio that is dropped and not sent to provider


class AudioTimeline:
    def __init__(self, error_cb: Optional[Callable[[str], None]] = None):
        # Store timeline event list, each event is a tuple of (type, duration)
        self.timeline: list[tuple[AudioTimelineEventType, int]] = []
        self.total_user_audio_duration = 0
        self.total_silence_audio_duration = 0
        self.total_dropped_audio_duration = 0
        self.error_cb = error_cb

    def add_user_audio(self, duration_ms: int):
        """Add user audio

        Args:
            duration_ms: Audio duration in milliseconds
        """
        if duration_ms <= 0:
            return

        if self.timeline and self.timeline[-1][0] == AudioTimelineEventType.USER_AUDIO:
            # Merge adjacent user audio events
            self.timeline[-1] = (
                AudioTimelineEventType.USER_AUDIO,
                self.timeline[-1][1] + duration_ms,
            )
        else:
            self.timeline.append((AudioTimelineEventType.USER_AUDIO, duration_ms))

        self.total_user_audio_duration += duration_ms

    def add_silence_audio(self, duration_ms: int):
        """Add silence audio

        Args:
            duration_ms: Silence duration in milliseconds
        """
        if duration_ms <= 0:
            return

        if (
            self.timeline
            and self.timeline[-1][0] == AudioTimelineEventType.SILENCE_AUDIO
        ):
            # Merge adjacent silence events
            self.timeline[-1] = (
                AudioTimelineEventType.SILENCE_AUDIO,
                self.timeline[-1][1] + duration_ms,
            )
        else:
            self.timeline.append((AudioTimelineEventType.SILENCE_AUDIO, duration_ms))

        self.total_silence_audio_duration += duration_ms

    def add_dropped_audio(self, duration_ms: int):
        """Add dropped audio (audio not sent to provider)

        Args:
            duration_ms: Dropped audio duration in milliseconds
        """
        if duration_ms <= 0:
            return

        if (
            self.timeline
            and self.timeline[-1][0] == AudioTimelineEventType.DROPPED_AUDIO
        ):
            # Merge adjacent dropped audio events
            self.timeline[-1] = (
                AudioTimelineEventType.DROPPED_AUDIO,
                self.timeline[-1][1] + duration_ms,
            )
        else:
            self.timeline.append((AudioTimelineEventType.DROPPED_AUDIO, duration_ms))

        self.total_dropped_audio_duration += duration_ms

    def get_audio_duration_before_time(self, time_ms: int) -> int:
        """
        Calculate the real audio timestamp from provider's timestamp.

        This method converts provider's timestamp to real audio timeline position by:
        - Adding dropped audio (exists in real world but not sent to provider)
        - Subtracting silence audio (sent to provider but not real user audio)

        Real audio timestamp = provider timestamp + dropped audio - silence

        Timeline diagram:
        Timeline: [DROPPED:3000ms] [USER:1000ms] [SILENCE:500ms] [USER:2000ms]
        Provider Time:              0           1000          1500          3500
        Real Audio Time: 0       3000         4000         (4000)        6000

        When provider returns 1500ms (after silence):
        - Real audio time = 3000 (dropped) + 1000 (first user) = 4000ms

        Examples:
        - get_audio_duration_before_time(0)    -> 3000ms (dropped audio before provider's first audio)
        - get_audio_duration_before_time(500)  -> 3500ms (dropped + 500ms user audio)
        - get_audio_duration_before_time(1000) -> 4000ms (dropped + 1000ms user audio)
        - get_audio_duration_before_time(1500) -> 4000ms (silence excluded)
        - get_audio_duration_before_time(2000) -> 4500ms (dropped + 1000 + 500 from second user)

        Args:
            time_ms: The timestamp from provider in milliseconds

        Returns:
            The real audio timeline position in milliseconds (only counting real audio)
        """
        if time_ms < 0:
            if self.error_cb is not None:
                try:
                    self.error_cb(f"Requested time {time_ms}ms is less than 0")
                except Exception:
                    # Silently ignore callback errors to keep returning result normally
                    pass
            # When requested time is less than 0, return 0
            return 0

        # Calculate total timeline duration (excluding dropped audio)
        total_timeline_duration = (
            self.total_user_audio_duration + self.total_silence_audio_duration
        )

        # Check if requested time exceeds timeline range
        if time_ms > total_timeline_duration:
            if self.error_cb is not None:
                try:
                    self.error_cb(
                        f"Requested time {time_ms}ms exceeds timeline duration {total_timeline_duration}ms"
                    )
                except Exception:
                    # Silently ignore callback errors to keep returning result normally
                    pass
            # When exceeding range, return total real audio duration (user + dropped)
            return self.total_user_audio_duration + self.total_dropped_audio_duration

        real_audio_time = 0  # Real audio timeline (user audio + dropped audio)
        provider_time = 0  # Provider timeline (user audio + silence)

        # Iterate through timeline to calculate real audio timestamp
        for event_type, duration in self.timeline:
            if event_type == AudioTimelineEventType.DROPPED_AUDIO:
                # Dropped audio: exists in real world, adds to real audio time
                # but not counted in provider time
                real_audio_time += duration
            elif event_type == AudioTimelineEventType.USER_AUDIO:
                # User audio: sent to provider and is real audio
                # Check if this segment crosses the target time
                if provider_time + duration > time_ms:
                    # Only add the partial duration
                    partial_duration = time_ms - provider_time
                    real_audio_time += partial_duration
                    break

                # Full segment is before target time
                provider_time += duration
                real_audio_time += duration

                # Check if we've exactly reached the target
                if provider_time >= time_ms:
                    break
            elif event_type == AudioTimelineEventType.SILENCE_AUDIO:
                # Silence: sent to provider but NOT real audio
                # Only advances provider time, not real audio time
                if provider_time + duration > time_ms:
                    # Target time is within this silence segment
                    # Don't add any audio duration
                    break

                # Full silence segment is before target time
                provider_time += duration
                # real_audio_time stays the same (silence excluded)

                if provider_time >= time_ms:
                    break

        return real_audio_time

    def get_total_user_audio_duration(self) -> int:
        """
        Get total duration of all user audio received from the user.
        This includes both audio sent to provider (USER_AUDIO) and audio dropped (DROPPED_AUDIO).

        This is typically used before reset to record the total user audio received in this session.

        Returns:
            Total duration of user audio in milliseconds (USER_AUDIO + DROPPED_AUDIO)
        """
        return self.total_user_audio_duration + self.total_dropped_audio_duration

    def reset(self):
        self.timeline = []
        self.total_user_audio_duration = 0
        self.total_silence_audio_duration = 0
        self.total_dropped_audio_duration = 0
