# Speech Diarization Example

This example demonstrates how to pair the TEN voice pipeline with Speechmatics speaker diarization. It reuses the Agora RTC ingress and message collector components from the voice assistant demo, but swaps in the `speechmatics_diarization_python` extension and a small controller that enriches transcripts with speaker labels.

## Components

- `agora_rtc`: streams audio into the pipeline.
- `speechmatics_diarization_python`: realtime ASR with speaker diarization metadata.
- `main_python`: formats diarized segments and forwards them to the message collector.
- `message_collector2`: pushes transcript updates to the UI via RTM.
- `streamid_adapter`: preserves Agora stream IDs for the ASR node.

Set the `SPEECHMATICS_API_KEY` environment variable before starting the graph. The default configuration targets English at 16 kHz, but you can adjust the language, diarization mode, and sensitivity in `property.json`.
