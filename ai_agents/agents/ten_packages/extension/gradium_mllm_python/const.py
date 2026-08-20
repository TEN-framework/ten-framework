"""
Constants for the Gradium real-time speech-to-speech translation extension.

Message-type strings and sample-rate/frame constants mirror
gradium_asr_python/const.py, which is already wired up against Gradium's
real ASR websocket API.
"""

MODULE_NAME_MLLM = "mllm"

WS_MSG_TYPE_SETUP = "setup"
WS_MSG_TYPE_READY = "ready"
WS_MSG_TYPE_AUDIO = "audio"
WS_MSG_TYPE_TEXT = "text"
WS_MSG_TYPE_VAD = "vad"
WS_MSG_TYPE_END = "end_of_stream"
WS_MSG_TYPE_ERROR = "error"

GRADIUM_INPUT_SAMPLE_RATE = 24000
GRADIUM_CHANNELS = 1
GRADIUM_BITS_PER_SAMPLE = 16
