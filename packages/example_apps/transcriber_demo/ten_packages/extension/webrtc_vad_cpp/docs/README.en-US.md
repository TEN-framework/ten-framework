# WebRTC VAD C++ Extension

## Overview

WebRTC VAD (Voice Activity Detection) extension written in C++ for TEN Framework.

## Features

- Real-time voice activity detection based on WebRTC VAD algorithm
- Supports multiple sample rates (8kHz, 16kHz, 32kHz, 48kHz)
- Supports multiple frame lengths (10ms, 20ms, 30ms)
- Adjustable detection sensitivity (mode 0-3)
- Low latency and low resource consumption
- Forwards original audio frames while outputting VAD results

## VAD Modes

The extension supports 4 sensitivity modes:

- **Mode 0**: Quality priority - Least aggressive, low false positives, may miss some speech
- **Mode 1**: Low aggressive - Balanced mode
- **Mode 2**: Medium aggressive - Default mode, recommended for most scenarios
- **Mode 3**: High aggressive - Most aggressive, high detection rate, may have more false positives

## Configuration

Configure the VAD mode in `property.json`:

```json
{
  "mode": 2
}
```

## Input and Output

### Input

- **Audio Frame (audio_frame)**:
  - Sample rate: 8000, 16000, 32000, or 48000 Hz
  - Bit depth: 16-bit (2 bytes per sample)
  - Frame length: 10ms, 20ms, or 30ms
  - Channels: Supports mono and multi-channel (uses first channel for multi-channel)

### Output

- **Audio Frame (audio_frame)**: Forwards original audio frame with added VAD detection result properties
  - `is_speech` (bool): true indicates speech detected, false indicates silence/noise
  - `frame_name` (string): Audio frame name

## Usage Example

Use this extension in your TEN application graph configuration:

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "webrtc_vad",
      "addon": "webrtc_vad_cpp",
      "property": {
        "mode": 2
      }
    }
  ],
  "connections": [
    {
      "extension": "audio_source",
      "audio_frame": [
        {
          "name": "audio_frame",
          "dest": [
            {
              "extension": "webrtc_vad"
            }
          ]
        }
      ]
    },
    {
      "extension": "webrtc_vad",
      "audio_frame": [
        {
          "name": "audio_frame",
          "dest": [
            {
              "extension": "downstream_processor"
            }
          ]
        }
      ]
    }
  ]
}
```

## Quick Start

### Prerequisites

- TEN Framework 0.11.30 or higher
- C++ compiler with C++11 support or higher

### Installation

Follow the TEN Framework package installation guide.

## Technical Details

### WebRTC VAD Algorithm

This extension uses a simplified version of the WebRTC VAD algorithm:

1. **Energy Calculation**: Computes RMS (Root Mean Square) energy of audio frames
2. **Threshold Detection**: Sets different energy thresholds based on mode
3. **Smoothing**: Uses consecutive frame history for state smoothing to reduce jitter

### Performance Characteristics

- **Low Latency**: Frame-by-frame processing, latency is only one frame duration (10-30ms)
- **Low Resource**: Pure energy calculation, no machine learning models required
- **High Efficiency**: C/C++ implementation, suitable for real-time applications

## License

This package is part of the TEN Framework project and follows Apache License 2.0.
