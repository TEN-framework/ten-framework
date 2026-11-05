#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
DUMP_FILE_NAME = "blaze_stt_in.pcm"
MODULE_NAME_ASR = "asr"

# Realtime STT WebSocket path, appended to the configured base_url.
STT_REALTIME_PATH = "/v1/stt/realtime"

# Cap on buffered audio while the connection is down so a long outage
# cannot grow memory unbounded.
AUDIO_BUFFER_BYTE_LIMIT = 10 * 1024 * 1024  # 10 MB

# Reconnect backoff for transient failures. Retries are unlimited (the ASR
# contract default); each wait doubles and is capped.
RECONNECT_BASE_DELAY_SECONDS = 0.5
RECONNECT_MAX_DELAY_SECONDS = 8.0

# Seconds to wait for the server's "ready" message after sending the token.
READY_TIMEOUT_SECONDS = 10.0

# WebSocket close code Blaze uses after "failed-authentication".
WS_CLOSE_POLICY_VIOLATION = 1008
