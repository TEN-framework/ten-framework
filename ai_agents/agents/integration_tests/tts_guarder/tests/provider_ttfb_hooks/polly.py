"""Observe Polly's supported SDK invocation and AudioStream chunks."""

from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module("ten_packages.extension.polly_tts.polly_tts")
        library = module.boto3

        class Stream:
            def __init__(self, stream):
                self.stream = stream

            def iter_chunks(self, *args, **kwargs):
                for chunk in self.stream.iter_chunks(*args, **kwargs):
                    reference.capture("audio", chunk)
                    yield chunk

            def close(self):
                return self.stream.close()

        class Client:
            def __init__(self, client):
                self.client = client

            def synthesize_speech(self, *args, **kwargs):
                reference.capture("text", kwargs.get("Text", ""))
                response = dict(self.client.synthesize_speech(*args, **kwargs))
                if "AudioStream" in response:
                    response["AudioStream"] = Stream(response["AudioStream"])
                return response

            def __getattr__(self, name):
                return getattr(self.client, name)

        class Session:
            def __init__(self, *args, **kwargs):
                self.session = library.Session(*args, **kwargs)

            def client(self, *args, **kwargs):
                return Client(self.session.client(*args, **kwargs))

            def __getattr__(self, name):
                return getattr(self.session, name)

        class Library:
            def __getattr__(self, name):
                return getattr(library, name)

        binding = Library()
        binding.Session = Session
        patch.setattr(module, "boto3", binding)
