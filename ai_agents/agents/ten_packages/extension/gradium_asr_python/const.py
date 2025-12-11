"""
Constants for Gradium ASR extension.
"""

EXTENSION_NAME = "gradium_asr_python"
MODULE_NAME_ASR = "asr"
PROPERTY_FILE_NAME = "property.json"
CMD_PROPERTY_NAME = "cmd"
CMD_IN_FLUSH = "flush"

WS_MSG_TYPE_SETUP = "setup"
WS_MSG_TYPE_READY = "ready"
WS_MSG_TYPE_AUDIO = "audio"
WS_MSG_TYPE_TEXT = "text"
WS_MSG_TYPE_VAD = "vad"
WS_MSG_TYPE_END = "end_of_stream"

GRADIUM_SAMPLE_RATE = 24000
GRADIUM_CHANNELS = 1
GRADIUM_BITS_PER_SAMPLE = 16
GRADIUM_FRAME_SIZE = 1920
