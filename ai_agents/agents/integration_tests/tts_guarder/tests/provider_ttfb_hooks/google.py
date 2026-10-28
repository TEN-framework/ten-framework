"""Observe Google's public lazy text iterator and nonempty audio responses."""

from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module("ten_packages.extension.google_tts_python.google_tts")
        original = module.texttospeech.TextToSpeechClient

        class Client(original):
            def streaming_synthesize(self, requests, *args, **kwargs):
                def inputs():
                    for request in requests:
                        if getattr(getattr(request, "input", None), "text", ""):
                            reference.capture("text")
                        yield request

                for response in super().streaming_synthesize(inputs(), *args, **kwargs):
                    reference.capture("audio", response.audio_content)
                    yield response

        patch.setattr(module.texttospeech, "TextToSpeechClient", Client)
