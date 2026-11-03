"""Observe the public HTTP adapter input and first nonempty audio output."""

from functools import wraps
from importlib import import_module

from ten_ai_base.struct import TTS2HttpResponseEventType


class HttpAdapterHook:
    module_name = ""
    client_class = ""

    def install(self, patch, reference, config):
        client = getattr(import_module(self.module_name), self.client_class)
        original = client.get

        @wraps(original)
        def observed_get(instance, text, request_id, *args, **kwargs):
            if text:
                reference.capture("text", request_id=request_id)
            stream = original(instance, text, request_id, *args, **kwargs)

            async def audio_events():
                async for audio, event in stream:
                    if event == TTS2HttpResponseEventType.RESPONSE and audio:
                        reference.capture("audio", request_id=request_id)
                    yield audio, event

            return audio_events()

        patch.setattr(client, "get", observed_get)
