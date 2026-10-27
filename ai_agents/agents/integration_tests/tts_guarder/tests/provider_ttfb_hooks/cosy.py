"""Observe DashScope public streaming_call and first nonempty audio callback."""

from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module("ten_packages.extension.cosy_tts_python.cosy_tts")
        original = module.SpeechSynthesizer

        class Callback(module.ResultCallback):
            def __init__(self, callback):
                self.callback = callback

            def on_data(self, data):
                reference.capture("audio", data)
                return self.callback.on_data(data)

            def on_open(self):
                return self.callback.on_open()

            def on_close(self):
                return self.callback.on_close()

            def on_complete(self):
                return self.callback.on_complete()

            def on_error(self, message):
                return self.callback.on_error(message)

            def on_event(self, message):
                return self.callback.on_event(message)

        class Synthesizer(original):
            def __init__(self, *args, **kwargs):
                kwargs["callback"] = Callback(kwargs["callback"])
                super().__init__(*args, **kwargs)

            def streaming_call(self, text, *args, **kwargs):
                reference.capture("text", text)
                return super().streaming_call(text, *args, **kwargs)

        patch.setattr(module, "SpeechSynthesizer", Synthesizer)
