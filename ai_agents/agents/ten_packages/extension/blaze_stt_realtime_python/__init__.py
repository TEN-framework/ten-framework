"""
Blaze Realtime STT Extension for TEN Framework

This extension provides realtime (streaming) Speech-to-Text (STT) functionality
using the Blaze realtime WebSocket API. Importing the package registers the
extension addon with the TEN runtime.
"""

try:
    from . import (
        addon,
    )  # noqa: F401  (registers the addon with the TEN runtime)
except ImportError:
    # `ten_runtime` is only available when running inside the TEN runtime.
    # Allow importing the package without it (e.g. for unit tests that exercise
    # the pure-Python BlazeRealtimeClient).
    pass

__version__ = "1.0.0"
