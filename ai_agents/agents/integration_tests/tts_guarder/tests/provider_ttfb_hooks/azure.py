"""Independent native mock SDK reference; no production SDK timing access."""

import ctypes
from threading import RLock


class NativeReference:
    def __init__(self, library):
        self.library = library
        self.lock = RLock()
        self.requests = {}
        for name in ["azure_mock_text_sent_ns", "azure_mock_first_audio_ns"]:
            function = getattr(library, name)
            function.argtypes = [ctypes.c_int]
            function.restype = ctypes.c_int64
        library.azure_mock_synthesis_count.argtypes = []
        library.azure_mock_synthesis_count.restype = ctypes.c_int

    def begin_request(self, request_id):
        with self.lock:
            self.requests.setdefault(
                request_id, self.library.azure_mock_synthesis_count()
            )

    def elapsed_ms(self, request_id):
        with self.lock:
            index = self.requests.get(request_id)
            if index is None:
                return None
            sent = self.library.azure_mock_text_sent_ns(index)
            received = self.library.azure_mock_first_audio_ns(index)
            if sent <= 0 or received < sent:
                return None
            return (received - sent) / 1_000_000


class Hook:
    def install(self, patch, reference, config):
        path = config.get("ttfb_mock_sdk_library")
        if not path:
            raise ValueError(
                "Azure native SDK cannot be intercepted by a Python transport hook. "
                "Independent reference requires the compile-time mock SDK library; "
                "set ttfb_mock_sdk_library only for that prepared mock app."
            )
        return NativeReference(ctypes.CDLL(path))
