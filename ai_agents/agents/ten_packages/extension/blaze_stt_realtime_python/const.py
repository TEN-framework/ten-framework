"""Constants for the Blaze realtime STT extension."""

MODULE_NAME_ASR = "asr"

DUMP_FILE_NAME = "blaze_stt_realtime_in.pcm"

# Realtime websocket endpoint appended to the configured base URL.
REALTIME_ENDPOINT = "/v1/stt/realtime"

# Default base URL used when none is configured.
DEFAULT_API_URL = "http://localhost:8000"

# Seconds to wait for the server "ready" handshake before failing the
# connection. The websocket-level `timeout` (default 3600s) is the overall
# session length, which is far too long to block on the initial handshake.
DEFAULT_HANDSHAKE_TIMEOUT = 10
