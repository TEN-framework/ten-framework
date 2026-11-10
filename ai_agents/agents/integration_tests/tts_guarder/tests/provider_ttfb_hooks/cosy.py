"""Observe DashScope public streaming_call and first nonempty audio callback."""

from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module(
            "ten_packages.extension.cosy_tts_python.cosy_tts"
        )
        original_call = module.SpeechSynthesizer.streaming_call
        original_borrow = module.SpeechSynthesizerObjectPool.borrow_synthesizer

        class Callback(module.ResultCallback):
            def __init__(self, callback):
                self.callback = callback
                self.request_id = getattr(callback, "request_id", None)

            def on_data(self, data):
                request_id = getattr(self.callback, "request_id", None)
                reference.capture("audio", data, request_id=request_id)
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

        def borrow(pool, *args, **kwargs):
            kwargs["callback"] = Callback(kwargs["callback"])
            return original_borrow(pool, *args, **kwargs)

        def streaming_call(synthesizer, text, *args, **kwargs):
            callback = getattr(synthesizer, "callback", None)
            request_id = getattr(callback, "request_id", None)
            reference.capture("text", text, request_id=request_id)
            return original_call(synthesizer, text, *args, **kwargs)

        patch.setattr(
            module.SpeechSynthesizerObjectPool, "borrow_synthesizer", borrow
        )
        patch.setattr(
            module.SpeechSynthesizer, "streaming_call", streaming_call
        )
