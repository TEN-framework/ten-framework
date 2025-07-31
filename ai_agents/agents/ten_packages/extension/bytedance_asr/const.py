CMD_IN_EVENT = "ten_event"
EVENTTYPE_START = "start"
CMD_PROPERTY_TASK_INFO = "taskInfo"
CMD_PROPERTY_PAYLOAD = "payload"
FINALIZE_MODE_DISCONNECT = "disconnect"
FINALIZE_MODE_MUTE_PKG = "mute_pkg"
DUMP_FILE_NAME = "bytedance_asr_in.pcm"
STREAM_ID = "stream_id"
REMOTE_USER_ID = "remote_user_id"
MODULE_NAME_ASR = "asr"

# Bytedance ASR Error Codes
# Reference: https://www.volcengine.com/docs/6561/80818#_3-3-%E9%94%99%E8%AF%AF%E7%A0%81
BYTEDANCE_ERROR_CODES = {
    1000: "SUCCESS",  # 成功
    1001: "INVALID_REQUEST_PARAMS",  # 请求参数无效
    1002: "ACCESS_DENIED",  # 无访问权限
    1003: "RATE_LIMIT_EXCEEDED",  # 访问超频
    1004: "QUOTA_EXCEEDED",  # 访问超额
    1005: "SERVER_BUSY",  # 服务器繁忙
    1010: "AUDIO_TOO_LONG",  # 音频过长
    1011: "AUDIO_TOO_LARGE",  # 音频过大
    1012: "INVALID_AUDIO_FORMAT",  # 音频格式无效
    1013: "AUDIO_SILENT",  # 音频静音
    1020: "RECOGNITION_WAIT_TIMEOUT",  # 识别等待超时
    1021: "RECOGNITION_TIMEOUT",  # 识别处理超时
    1022: "RECOGNITION_ERROR",  # 识别错误
    1099: "UNKNOWN_ERROR",  # 未知错误
    2001: "WEBSOCKET_CONNECTION_ERROR",  # WebSocket连接错误（自定义）
    2002: "DATA_TRANSMISSION_ERROR",  # 数据传输错误（自定义）
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
