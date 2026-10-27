"""Observe OCI's public synthesis call and audio body, before EOF buffering."""

from copy import copy
from importlib import import_module


class AudioDetector:
    def __init__(self):
        self.prefix = b""

    def feed(self, chunk):
        self.prefix += chunk
        if not self.prefix:
            return False
        if len(self.prefix) < 4 and b"RIFF".startswith(self.prefix):
            return False
        if not self.prefix.startswith(b"RIFF"):
            return True
        offset = 12
        while offset + 8 <= len(self.prefix):
            name = self.prefix[offset : offset + 4]
            size = int.from_bytes(self.prefix[offset + 4 : offset + 8], "little")
            if name == b"data":
                return len(self.prefix) > offset + 8
            offset += 8 + size + size % 2
        return False


class Hook:
    def install(self, patch, reference, config):
        module = import_module("ten_packages.extension.oracle_tts_python.oracle_tts")
        original = module.oci.ai_speech.AIServiceSpeechClient

        class Body:
            def __init__(self, data):
                self.data = data
                self.decoder = AudioDetector()

            def iter_content(self, chunk_size):
                if hasattr(self.data, "iter_content"):
                    chunks = self.data.iter_content(chunk_size=chunk_size)
                elif hasattr(self.data, "read"):
                    chunks = iter(lambda: self.data.read(chunk_size), b"")
                else:
                    chunks = [
                        (
                            self.data.content
                            if hasattr(self.data, "content")
                            else bytes(self.data)
                        )
                    ]
                for chunk in chunks:
                    reference.capture("audio", chunk, self.decoder.feed)
                    yield chunk

        class Client(original):
            def synthesize_speech(self, *args, **kwargs):
                details = kwargs.get("synthesize_speech_details")
                reference.capture("text", getattr(details, "text", ""))
                response = copy(super().synthesize_speech(*args, **kwargs))
                response.data = Body(response.data)
                return response

        patch.setattr(module.oci.ai_speech, "AIServiceSpeechClient", Client)
