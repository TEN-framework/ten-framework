# Audio File Player Extension

## Overview

This is a TEN Framework audio file player extension written in Python. It receives audio file paths through the `start_play` command, loads wav/mp3/pcm audio files, automatically converts them to 16000Hz PCM format, and sends audio frames at 10ms intervals.

## Features

- ✅ Support multiple audio formats: WAV, MP3, PCM, OGG, FLAC, M4A
- ✅ Automatic sample rate conversion to 16000Hz
- ✅ Automatic conversion to mono channel
- ✅ Send audio frames at 10ms intervals (160 samples per frame)
- ✅ Support loop playback
- ✅ Command-based playback control (start/stop)
- ✅ Async implementation for excellent performance

## Supported Commands

### `start_play`

Start playing an audio file.

**Command Properties**:

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `file_path` | string | ✅ | - | Audio file path (absolute or relative) |
| `loop_playback` | bool | ❌ | false | Whether to loop playback |

**Example**:

```json
{
  "name": "start_play",
  "property": {
    "file_path": "/path/to/your/audio.wav",
    "loop_playback": false
  }
}
```

### `stop_play`

Stop playing audio.

**Example**:

```json
{
  "name": "stop_play"
}
```

## Output

### Audio Frame Output

- **Name**: `pcm_frame`
- **Format**: INTERLEAVE
- **Sample Rate**: 16000Hz
- **Bit Depth**: 16-bit (2 bytes per sample)
- **Channels**: 1 (mono)
- **Samples Per Frame**: 160 samples (10ms @ 16kHz)
- **Bytes Per Frame**: 320 bytes

## Usage Examples

### 1. Use in Graph

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "audio_player",
      "addon": "audio_file_player_python"
    }
  ]
}
```

### 2. Send Play Command

Send a `start_play` command to play audio:

```json
{
  "name": "start_play",
  "property": {
    "file_path": "/path/to/audio.mp3",
    "loop_playback": true
  }
}
```

### 3. Stop Playback

```json
{
  "name": "stop_play"
}
```

## Technical Details

### Audio Processing Flow

1. **Load Audio**: Use `pydub` library to load various audio formats
2. **Format Conversion**:
   - Convert sample rate to 16000Hz
   - Convert to mono channel
   - Format as 16-bit PCM
3. **Frame Splitting**: Split audio data into 10ms (160 samples) frames
4. **Timed Sending**: Use asyncio to send audio frames at 10ms intervals

### Audio Parameter Calculation

- Sample Rate: 16000 Hz
- Frame Duration: 10 ms
- Samples Per Frame: 16000 × 0.01 = 160 samples
- Bytes Per Sample: 2 bytes (16-bit)
- Bytes Per Frame: 160 × 2 = 320 bytes

## Dependencies

- `ten_runtime_python` >= 0.11.25
- `pydub` >= 0.25.1

### Install Dependencies

```bash
pip install pydub
```

**Note**: For MP3 format support, you also need to install `ffmpeg`:

```bash
# Ubuntu/Debian
sudo apt-get install ffmpeg

# macOS
brew install ffmpeg

# Windows
# Download and install ffmpeg, then add to PATH
```

## License

This project is licensed under the Apache License 2.0.

## Authors

TEN Framework Team

## Changelog

### v0.11.25 (2025-10-29)

- Initial release
- Support wav/mp3/pcm and other audio formats
- Implement 16kHz PCM conversion and 10ms frame interval sending
- Support loop playback and command control
