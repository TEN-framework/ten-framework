# VTT Recorder Extension

A Node.js/TypeScript extension for TEN Framework that records audio streams and ASR (Automatic Speech Recognition) results, generating standard WebVTT subtitle files.

## Features

- **Audio Recording**: Records PCM audio frames to WAV files
- **VTT Generation**: Creates WebVTT subtitle files from ASR results
- **Session Management**: Manages multiple recording sessions with metadata
- **Multi-format Export**: Saves transcriptions in VTT, JSON, and WAV formats
- **Real-time Processing**: Handles streaming audio and text data in real-time

## Architecture

```text
┌─────────────────┐
│  Audio Source   │ (e.g., Audio Player, Microphone)
└────────┬────────┘
         │ pcm_frame
         ↓
┌─────────────────┐
│                 │
│  VTT Recorder   │ ← asr_result ← ASR Engine
│   (Node.js)     │
│                 │
└────────┬────────┘
         │
         ↓
   ┌─────────────────┐
   │ Local Storage   │
   ├─────────────────┤
   │ • audio.wav     │
   │ • transcript.vtt│
   │ • metadata.json │
   └─────────────────┘
```

## Commands

### `start_recording`

Starts a new recording session.

**Response:**

- `session_id`: Unique identifier for the session
- `detail`: Status message

### `stop_recording`

Stops the current recording session and saves all files.

**Response:**

- `session_id`: Session identifier
- `duration`: Recording duration in milliseconds
- `segments`: Number of subtitle segments
- `words`: Total word count

### `list_sessions`

Lists all recorded sessions.

**Response:**

- `sessions`: JSON array of session metadata
- `count`: Number of sessions

### `get_session`

Retrieves metadata for a specific session.

**Parameters:**

- `session_id`: Session identifier

**Response:**

- `metadata`: JSON object with session details

### `delete_session`

Deletes a recording session and all its files.

**Parameters:**

- `session_id`: Session identifier

## Input Data

### Audio Frames

- **Name**: `pcm_frame`
- **Format**: PCM audio data (16kHz, mono, 16-bit recommended)
- **Usage**: Automatically recorded when session is active

### ASR Results

- **Name**: `asr_result`
- **Properties**:
  - `text` (string): Transcribed text
  - `final` (bool): Whether this is a final result
- **Usage**: Automatically processed and converted to VTT segments

## Output Files

Each recording session creates a directory with the following files:

```text
recordings/
└── <session-id>/
    ├── audio.wav          # Recorded audio
    ├── transcript.vtt     # WebVTT subtitle file
    ├── transcript.json    # JSON format transcript
    └── metadata.json      # Session metadata
```

### WebVTT Format

```vtt
WEBVTT

1
00:00:00.000 --> 00:00:05.000
Hello, this is a test.

2
00:00:05.000 --> 00:00:10.000
The quick brown fox jumps over the lazy dog.
```

## Configuration

Edit `property.json` to customize:

```json
{
  "recordings_path": "./recordings"
}
```

## Dependencies

- `node-wav`: WAV file encoding (19KB)
- `uuid`: Session ID generation
- `ten-runtime-nodejs`: TEN Framework runtime

## Installation

```bash
cd ten_packages/extension/vtt_nodejs
npm install
npm run build
```

## Usage Example

### Integration with TEN Framework

Add to your app's `property.json`:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "nodes": [
          {
            "type": "extension",
            "name": "vtt_recorder",
            "addon": "vtt_nodejs"
          }
        ],
        "connections": [
          {
            "extension": "audio_source",
            "audio_frame": [
              {
                "name": "pcm_frame",
                "dest": [{"extension": "vtt_recorder"}]
              }
            ]
          },
          {
            "extension": "asr_engine",
            "data": [
              {
                "name": "asr_result",
                "dest": [{"extension": "vtt_recorder"}]
              }
            ]
          }
        ]
      }
    ]
  }
}
```

### Control Recording

```typescript
// Start recording
await tenEnv.sendCmd("start_recording");

// ... audio and ASR data flows automatically ...

// Stop recording
await tenEnv.sendCmd("stop_recording");
```

## Playback in Browser

Use the generated files in HTML:

```html
<audio controls>
  <source src="/recordings/<session-id>/audio.wav" type="audio/wav">
  <track src="/recordings/<session-id>/transcript.vtt"
         kind="subtitles"
         srclang="en"
         label="English">
</audio>
```

## Technical Details

### Audio Processing

- Supports multiple sample rates (auto-detected)
- Supports 8/16/24/32-bit PCM formats
- Converts to 16-bit WAV for compatibility
- Uses streaming to minimize memory usage

### VTT Generation

- Automatic sentence segmentation
- Maximum segment duration: 7 seconds
- Minimum segment duration: 1 second
- Text formatting (capitalization, punctuation)
- Timestamp synchronization with audio

### Session Management

- UUID-based session IDs
- Automatic metadata tracking
- Safe concurrent session handling
- Cleanup on error

## Performance

- **Memory**: Constant memory usage with streaming
- **Latency**: < 10ms per audio frame
- **Storage**: ~32 KB/s for 16kHz mono audio
- **CPU**: Minimal overhead (< 5%)

## Troubleshooting

### Recording not starting

- Check if another session is active
- Verify audio source is sending `pcm_frame` data

### VTT file is empty

- Ensure ASR results have `final=true`
- Check that ASR is sending to correct extension

### Large file sizes

- Default WAV format is uncompressed
- Consider post-processing compression if needed

## License

Apache License 2.0

## Support

For issues and questions, please refer to the TEN Framework documentation.
