import re
import time

PUNCTUATION_PATTERN = re.compile(r"[,，.。!！?？:：;；、]")


def remove_punctuation(text: str) -> str:
    return PUNCTUATION_PATTERN.sub("", text)


class TimeHelper:
    """
    A helper class for time calculations.

    This class provides a set of class methods to calculate the duration
    between two time points or from a given time point to the present.
    It supports returning durations in seconds or milliseconds.
    """

    @classmethod
    def duration(cls, start: float, end: float) -> float:
        """
        Calculate the duration between two time points.

        Args:
            start (float): The start time in seconds since the epoch.
            end (float): The end time in seconds since the epoch.

        Returns:
            float: The time difference between the two points in seconds.
        """

        return end - start

    @classmethod
    def duration_since(cls, start: float) -> float:
        """
        Calculate the duration from a given time point to now.

        Args:
            start (float): The start time in seconds since the epoch.

        Returns:
            float: The time difference from the start time to now in seconds.
        """

        return cls.duration(start, time.time())

    @classmethod
    def duration_ms(cls, start: float, end: float) -> int:
        """
        Calculate the duration between two time points in milliseconds.

        Args:
            start (float): The start time in seconds since the epoch.
            end (float): The end time in seconds since the epoch.

        Returns:
            int: The time difference between the two points in milliseconds.
        """

        return int((end - start) * 1000)

    @classmethod
    def duration_ms_since(cls, start: float) -> int:
        """
        Calculate the duration from a given time point to now in milliseconds.

        Args:
            start (float): The start time in seconds since the epoch.

        Returns:
            int: The time difference from the start time to now in milliseconds.
        """

        return cls.duration_ms(start, time.time())
