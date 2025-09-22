#!/usr/bin/env python
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Google Cloud Text-To-Speech API streaming sample application .

Example usage:
    python streaming_tts_quickstart.py
"""


def run_streaming_tts_quickstart():
    """Synthesizes speech from a stream of input text."""
    from google.cloud import texttospeech

    client = texttospeech.TextToSpeechClient()

    # See https://cloud.google.com/text-to-speech/docs/voices for all voices.
    streaming_config = texttospeech.StreamingSynthesizeConfig(
        voice=texttospeech.VoiceSelectionParams(
            name="en-US-Chirp3-HD-Charon",
            language_code="en-US",
        )
    )

    # Set the config for your stream. The first request must contain your config, and then each subsequent request must contain text.
    config_request = texttospeech.StreamingSynthesizeRequest(
        streaming_config=streaming_config
    )

    text_iterator = [
        "Hello there. ",
        "How are you ",
        "today? It's ",
        "such nice weather outside.",
    ]

    # Request generator. Consider using Gemini or another LLM with output streaming as a generator.
    def request_generator():
        yield config_request
        for text in text_iterator:
            yield texttospeech.StreamingSynthesizeRequest(
                input=texttospeech.StreamingSynthesisInput(text=text)
            )

    streaming_responses = client.streaming_synthesize(request_generator())

    for response in streaming_responses:
        print(f"Audio content size in bytes is: {len(response.audio_content)}")


if __name__ == "__main__":
    run_streaming_tts_quickstart()