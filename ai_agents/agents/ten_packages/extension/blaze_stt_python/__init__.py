"""
Blaze STT Extension for TEN Framework

This extension provides Speech-to-Text (STT) functionality using Blaze API.
Implements TEN framework extension interface.
"""

from .blaze_stt import BlazeSTTExtension, BlazeSTTConfig

__all__ = ["BlazeSTTExtension", "BlazeSTTConfig"]
__version__ = "1.0.0"

