DUMP_FILE_NAME = "assemblyai_asr_in.pcm"

# Standard TTS input message; when routed to this extension the agent's
# spoken reply becomes `agent_context` for the next user turn.
DATA_IN_TTS_TEXT_INPUT = "tts_text_input"

# Documented per-value limits.
AGENT_CONTEXT_MAX_CHARS = 1750
PROMPT_MAX_CHARS = 1750
MAX_KEYTERMS = 100
MAX_LANGUAGE_CODES = 10

# Accepted enum values.
VALID_ENCODINGS = frozenset({"pcm_s16le", "pcm_mulaw"})
VALID_MODES = frozenset({"min_latency", "balanced", "max_accuracy"})
VALID_VOICE_FOCUS = frozenset({"near-field", "far-field"})
VALID_PII_SUBSTITUTIONS = frozenset({"hash", "entity_name"})

# How long to wait for a final turn after ForceEndpoint before completing
# the finalize handshake anyway.
DEFAULT_FINALIZE_TIMEOUT_MS = 3000

# WebSocket close codes: 1008 = missing authorization / account issue.
FATAL_CLOSE_CODES = frozenset({1008})
# Handshake HTTP statuses that will not succeed on retry.
FATAL_HTTP_STATUSES = frozenset({"401", "402", "403"})
# Clean closures: reconnect silently, do not emit an error.
NORMAL_CLOSE_CODES = frozenset({1000, 1001})
