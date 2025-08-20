FINALIZE_MODE_DISCONNECT = "disconnect"
FINALIZE_MODE_MUTE_PKG = "mute_pkg"
DUMP_FILE_NAME = "bytedance_asr_in.pcm"

# Bytedance ASR Error Codes
# Reference: https://www.volcengine.com/docs/6561/80818#_3-3-%E9%94%99%E8%AF%AF%E7%A0%81
BYTEDANCE_ERROR_CODES = {
    1000: "SUCCESS",  # Success
    1001: "INVALID_REQUEST_PARAMS",  # Invalid request parameters
    1002: "ACCESS_DENIED",  # Access denied
    1003: "RATE_LIMIT_EXCEEDED",  # Rate limit exceeded
    1004: "QUOTA_EXCEEDED",  # Quota exceeded
    1005: "SERVER_BUSY",  # Server busy
    1010: "AUDIO_TOO_LONG",  # Audio too long
    1011: "AUDIO_TOO_LARGE",  # Audio too large
    1012: "INVALID_AUDIO_FORMAT",  # Invalid audio format
    1013: "AUDIO_SILENT",  # Audio is silent
    1020: "RECOGNITION_WAIT_TIMEOUT",  # Recognition wait timeout
    1021: "RECOGNITION_TIMEOUT",  # Recognition processing timeout
    1022: "RECOGNITION_ERROR",  # Recognition error
    1099: "UNKNOWN_ERROR",  # Unknown error
    2001: "WEBSOCKET_CONNECTION_ERROR",  # WebSocket connection error (custom)
    2002: "DATA_TRANSMISSION_ERROR",  # Data transmission error (custom)
}

# Error codes that require reconnection
RECONNECTABLE_ERROR_CODES = [
    1002,  # Access denied - token may be expired, retry may help
    1003,  # Rate limit exceeded - retry with backoff
    1004,  # Quota exceeded - may reset, retry later
    1005,  # Server busy - temporary issue, retry
    1020,  # Recognition wait timeout - temporary issue
    1021,  # Recognition timeout - temporary issue
    1022,  # Recognition error - may be temporary
    1099,  # Unknown error - may be recoverable
    2001,  # WebSocket connection error (custom)
    2002,  # Data transmission error (custom)
]

# Fatal error codes that should not trigger reconnection
FATAL_ERROR_CODES = [
    1001,  # Invalid request params - configuration issue, no point retrying
    1010,  # Audio too long - content issue, no point retrying
    1011,  # Audio too large - content issue, no point retrying
    1012,  # Invalid audio format - format issue, no point retrying
    1013,  # Audio silent - content issue, no point retrying
]

# Default workflow configuration
DEFAULT_WORKFLOW = "audio_in,resample,partition,vad,fe,decode,itn,nlu_punctuate"
