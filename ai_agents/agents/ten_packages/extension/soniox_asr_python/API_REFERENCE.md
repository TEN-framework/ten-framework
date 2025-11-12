# Soniox Websocket API Reference

## Authentication and configuration

Before sending audio, you must authenticate and configure the transcription session by sending a JSON message like this:


```json
{
  "api_key": "<SONIOX_API_KEY|SONIOX_TEMPORARY_API_KEY>",
  "model": "stt-rt-preview",
  "audio_format": "auto",
  "num_channels": 1,
  "sample_rate": 16000,
  "language_hints": ["zh", "en"],
  "context": "",
  "enable_speaker_diarization": false,
  "enable_language_identification": false,
  "enable_non_final_tokens": true,
  "max_non_final_tokens_duration_ms": 360,
  "enable_endpoint_detection": false,
  "client_reference_id": ""
}
```

`api_key`, `model`, `audio_format` are required, others are optional.

## Audio Streaming

After sending the initial configuration, begin streaming audio data:

Audio can be sent as binary WebSocket frames (preferred)
Alternatively, Base64-encoded audio can be sent as text messages (if binary is not supported)
The maximum duration of a stream is 65 minutes

## Ending the stream

To gracefully end a transcription session:

Send an empty WebSocket message (empty binary or text frame)
The server will return any final results, send a completion message, and close the connection

## Manual finalize

Send special message:

```json
{"type": "finalize"}
```

to trigger manual finalization.
Soniox will finalize all audio received up to that point.
All tokens associated with the finalized audio will be returned as is_final: true.
After the finalization is complete, the model returns a special token:
```json
{
  "text": "<fin>",
  "is_final": true
}
```
This marks the end of the finalize operation.


## KeepAlive

Send special message:

```json
{"type": "keepalive"}

to keep connection alive.
When there is no audio data, this message should be sent every 20 seconds.
You can send this more frequently.


## Response format

Soniox will send transcription responses in JSON format. Successful transcription responses follow this format:


```json
{
  "tokens": [
    {
      "text": "Hello",
      "start_ms": 600,
      "end_ms": 760,
      "confidence": 0.97,
      "is_final": true,
      "speaker": "1",
      "language": "en"
    }
  ],
  "final_audio_proc_ms": 760,
  "total_audio_proc_ms": 880
}
```

## Finished response

At the end of the stream, Soniox will send a final message indicating the session is complete:


```json
{
  "tokens": [],
  "final_audio_proc_ms": 1560,
  "total_audio_proc_ms": 1680,
  "finished": true
}
```
The server will then close the WebSocket connection.

## Error response

If an error occurs, the server will send an error response and immediately close the connection:


```json
{
  "tokens": [],
  "error_code": 503,
  "error_message": "Service is currently overloaded. Please retry your request..."
}
```

error_code is standard HTTP error code.