# Speechmatics Diarization Extension

This extension wraps the Speechmatics realtime ASR client and enables speaker diarization out of the box. It shares the same runtime dependencies as the `speechmatics_asr_python` extension, but automatically configures the transcription session with `diarization="speaker"` and exposes diarization metadata (speaker segments, labels, and timestamps) through the `ASRResult.metadata` payload.

## Configuration

The default `property.json` picks the Speechmatics region from the `params.uri` field (fallbacks to the global default when omitted) and expects the API key via the `SPEECHMATICS_API_KEY` environment variable. You can tune diarization behaviour either by updating the nested `speaker_diarization_config` object or by adding flattened keys (`speaker_sensitivity`, `max_speakers`, `prefer_current_speaker`) inside `params`.

```json
{
  "params": {
    "key": "${env:SPEECHMATICS_API_KEY|}",
    "language": "en",
    "sample_rate": 16000,
    "diarization": "speaker",
    "speaker_diarization_config": {
      "speaker_sensitivity": 0.6,
      "prefer_current_speaker": true,
      "max_speakers": 6
    }
  }
}
```

The extension publishes diarization segments alongside the raw transcript. Each `segments` entry in `metadata.diarization` contains the speaker label, channel (if provided), the aligned text, and millisecond timestamps, making it easy to forward to downstream processors or UI layers.
