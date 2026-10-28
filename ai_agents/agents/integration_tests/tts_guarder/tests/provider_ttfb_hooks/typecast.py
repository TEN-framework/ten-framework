"""Observe aiohttp body admission and Typecast's public SDK audio iterator."""

from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module(
            "ten_packages.extension.typecast_tts_python.typecast_tts"
        )
        library = module.aiohttp
        original_session = library.ClientSession
        original_client = module.AsyncTypecast

        def session(*args, **kwargs):
            trace = library.TraceConfig()

            async def sent(session, context, params):
                if params.chunk:
                    reference.capture("text")

            trace.on_request_chunk_sent.append(sent)
            kwargs["trace_configs"] = [*kwargs.get("trace_configs", []), trace]
            return original_session(*args, **kwargs)

        class Library:
            ClientSession = staticmethod(session)

            def __getattr__(self, name):
                return getattr(library, name)

        class Client(original_client):
            async def text_to_speech_stream(self, *args, **kwargs):
                async for chunk in super().text_to_speech_stream(*args, **kwargs):
                    reference.capture("audio", chunk)
                    yield chunk

        patch.setattr(module, "aiohttp", Library())
        patch.setattr(module, "AsyncTypecast", Client)
