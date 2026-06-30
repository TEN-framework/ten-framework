"""
Blaze TTS Extension for TEN Framework

This extension provides Text-to-Speech (TTS) functionality using Blaze API.
Implements TEN framework extension interface.
"""

from .blaze_tts import BlazeTTSExtension, BlazeTTSConfig

__all__ = ["BlazeTTSExtension", "BlazeTTSConfig"]
__version__ = "1.0.0"
