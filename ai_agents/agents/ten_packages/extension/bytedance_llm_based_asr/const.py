# Constants for Volcengine ASR LLM Extension

# File names
DUMP_FILE_NAME = "bytedance_llm_based_asr_in.pcm"

# Protocol version
PROTOCOL_VERSION = 0b0001

# Message types
MESSAGE_TYPE_CLIENT_FULL_REQUEST = 0b0001
MESSAGE_TYPE_CLIENT_AUDIO_ONLY_REQUEST = 0b0010
MESSAGE_TYPE_SERVER_FULL_RESPONSE = 0b1001
MESSAGE_TYPE_SERVER_ERROR_RESPONSE = 0b1111

# Message type specific flags
MESSAGE_TYPE_SPECIFIC_FLAGS_NO_SEQUENCE = 0b0000
MESSAGE_TYPE_SPECIFIC_FLAGS_POS_SEQUENCE = 0b0001
MESSAGE_TYPE_SPECIFIC_FLAGS_NEG_SEQUENCE = 0b0010
MESSAGE_TYPE_SPECIFIC_FLAGS_NEG_WITH_SEQUENCE = 0b0011
MESSAGE_TYPE_SPECIFIC_FLAGS_LAST_AUDIO = 0b0010  # Last audio packet flag

# Serialization types
SERIALIZATION_TYPE_NO_SERIALIZATION = 0b0000
SERIALIZATION_TYPE_JSON = 0b0001

# Compression types
COMPRESSION_TYPE_NO_COMPRESSION = 0b0000
COMPRESSION_TYPE_GZIP = 0b0001

# Error codes - Official Volcengine ASR LLM error codes
VOLCENGINE_ERROR_CODES = {
    # Success
    "SUCCESS": 20000000,
    # Client errors (4xxxxxxx)
    "UNKNOWN_CLIENT_ERROR": 45000000,  # Unknown client error (observed in logs)
    "INVALID_PARAMETER": 45000001,  # Invalid request parameters
    "EMPTY_AUDIO": 45000002,  # Empty audio data
    "PACKET_TIMEOUT": 45000081,  # Packet timeout
    "INVALID_AUDIO_FORMAT": 45000151,  # Invalid audio format
    # Server errors (5xxxxxxx)
    "INTERNAL_ERROR": 55000000,  # Internal server error
    "SERVER_BUSY": 55000031,  # Server busy/overloaded
}

# Reconnectable error codes - errors that can be retried
RECONNECTABLE_ERROR_CODES = {
    VOLCENGINE_ERROR_CODES[
        "UNKNOWN_CLIENT_ERROR"
    ],  # Unknown client error - may be temporary, retryable
    VOLCENGINE_ERROR_CODES[
        "EMPTY_AUDIO"
    ],  # Empty audio - data issue, retryable
    VOLCENGINE_ERROR_CODES[
        "PACKET_TIMEOUT"
    ],  # Packet timeout - network issue, retryable
    VOLCENGINE_ERROR_CODES[
        "INVALID_AUDIO_FORMAT"
    ],  # Invalid audio format - format issue, retryable
    VOLCENGINE_ERROR_CODES[
        "INTERNAL_ERROR"
    ],  # Internal error - server issue, retryable
    VOLCENGINE_ERROR_CODES[
        "SERVER_BUSY"
    ],  # Server busy - temporary overload, retryable
}

# Fatal error codes - errors that should not be retried
FATAL_ERROR_CODES = {
    VOLCENGINE_ERROR_CODES[
        "INVALID_PARAMETER"
    ],  # Invalid parameters - configuration issue, retry won't help
}

# Default values
DEFAULT_SAMPLE_RATE = 16000
DEFAULT_SEGMENT_DURATION_MS = 100
DEFAULT_END_WINDOW_SIZE_MS = 300
DEFAULT_MAX_RETRIES = 5
DEFAULT_BASE_DELAY = 0.3


def is_reconnectable_error(error_code: int) -> bool:
    """
    Check if an error code should trigger reconnection.

    Args:
        error_code: The error code to check

    Returns:
        True if the error should trigger reconnection, False otherwise
    """
    # Check exact matches first (includes all original RECONNECTABLE_ERROR_CODES)
    if error_code in RECONNECTABLE_ERROR_CODES:
        return True

    # Check range matches for specific error patterns
    # INTERNAL_ERROR: 550xxxxx (all server internal errors)
    # This covers all 550xxxxx errors, not just the specific 55000000
    if 55000000 <= error_code <= 55099999:
        return True

    return False
