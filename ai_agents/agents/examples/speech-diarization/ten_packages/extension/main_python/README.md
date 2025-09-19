# Diarization Control Extension

This lightweight control extension consumes the `asr_result` stream and formats Speechmatics diarization metadata into speaker-labelled messages for the TEN message collector. Each diarized speaker receives a stable `stream_id`, so the existing voice-assistant UI can render distinct participants while keeping the rest of the pipeline unchanged.

## Options

- `skip_partials`: Ignore non-final ASR hypotheses to avoid duplicate UI entries (default: `true`).
- `show_channel_labels`: Include channel identifiers in the rendered prefix when channel diarization is enabled (default: `true`).
- `speaker_prefix`: Template used to prefix each transcript, where `{}` is replaced with the speaker label (default: `[{}] `).
