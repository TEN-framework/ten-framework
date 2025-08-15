#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#


class AudioTimeline:
    def __init__(self):
        # Store timeline events list, each event is a tuple of (type, duration)
        self.timeline = []

    def add_user_audio(self, duration_ms: int):
        """Add user audio

        Args:
            duration_ms: Audio duration (milliseconds)
        """
        if duration_ms <= 0:
            return

        if self.timeline and self.timeline[-1][0] == "user_audio":
            # Merge adjacent user audio events
            self.timeline[-1] = (
                "user_audio",
                self.timeline[-1][1] + duration_ms,
            )
        else:
            self.timeline.append(("user_audio", duration_ms))

    def add_silence_audio(self, duration_ms: int):
        """Add silence audio

        Args:
            duration_ms: Silence duration (milliseconds)
        """
        if duration_ms <= 0:
            return

        if self.timeline and self.timeline[-1][0] == "silence_audio":
            # Merge adjacent silence events
            self.timeline[-1] = (
                "silence_audio",
                self.timeline[-1][1] + duration_ms,
            )
        else:
            self.timeline.append(("silence_audio", duration_ms))

    def get_audio_duration_before_time(self, time_ms: int) -> int:
        total_duration = 0
        current_time = 0
        for event, duration in self.timeline:
            if current_time >= time_ms:
                break
            if event == "user_audio":
                if current_time + duration < time_ms:
                    total_duration += duration
                else:
                    total_duration += max(0, time_ms - current_time)
                    break
            current_time += duration
        return total_duration

    def get_total_user_audio_duration(self) -> int:
        return sum(
            duration
            for event, duration in self.timeline
            if event == "user_audio"
        )

    def reset(self):
        self.timeline = []
