"""Observe Typecast SDK text and raw WAV independently of the adapter."""

from functools import wraps
from importlib import import_module
from io import BytesIO
import wave


class Hook:
    def install(self, patch, reference, config):
        client = import_module("typecast").AsyncTypecast
        original = client.text_to_speech_stream

        @wraps(original)
        async def observed_stream(instance, request, *args, **kwargs):
            # The HTTP extension serializes requests; snapshot before any await.
            request_id = reference.current_request()
            if request_id is not None and request.text.strip():
                reference.capture("text", request_id=request_id)
            prefix = bytearray()
            found_audio = False

            def has_pcm(chunk):
                nonlocal found_audio
                if found_audio or not chunk:
                    return False
                prefix.extend(chunk)
                try:
                    with wave.open(BytesIO(prefix), "rb") as audio:
                        found_audio = (
                            audio.getnchannels() == 1
                            and audio.getsampwidth() == 2
                            and audio.getframerate() == 32000
                            and len(audio.readframes(1)) == 2
                        )
                except (EOFError, wave.Error):
                    return False
                return found_audio

            stream = original(instance, request, *args, **kwargs)
            try:
                async for chunk in stream:
                    if request_id is not None:
                        reference.capture(
                            "audio", chunk, has_pcm, request_id=request_id
                        )
                    yield chunk
            finally:
                await stream.aclose()

        patch.setattr(client, "text_to_speech_stream", observed_stream)
