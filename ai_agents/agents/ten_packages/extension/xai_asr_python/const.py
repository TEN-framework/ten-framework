DUMP_FILE_NAME = "xai_asr_in.pcm"
MODULE_NAME_ASR = "asr"

# Cap on the buffered-audio-frames byte size when the connection is down so a
# long outage cannot grow memory unbounded.
AUDIO_BUFFER_BYTE_LIMIT = 10 * 1024 * 1024  # 10 MB

# Reconnect ceiling overrides the manager default (4) — xAI ASR sessions
# tolerate longer transient outages because the audio buffer holds 10 MB.
RECONNECT_MAX_ATTEMPTS = 10
