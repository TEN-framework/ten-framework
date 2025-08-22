# Tencent TTS Python Extension

This extension provides Tencent Cloud Text-to-Speech (TTS) capabilities using the AsyncTTS2BaseExtension framework.

## Features

- Streaming TTS synthesis using Tencent Cloud API
- Support for multiple voice types and models
- Configurable sample rates and audio formats
- Emotion and speed control
- Audio dump functionality for debugging
- Comprehensive error handling

## Configuration

Set the following environment variables:
- `TENCENT_TTS_APP_ID`: Your Tencent Cloud App ID
- `TENCENT_TTS_SECRET_ID`: Your Tencent Cloud Secret ID
- `TENCENT_TTS_SECRET_KEY`: Your Tencent Cloud Secret Key

## Properties

### Top-level Properties
- `dump`: Enable audio dump for debugging (type: bool)
- `dump_path`: Path for audio dump files (type: string)

### TTS Parameters (nested under `params`)
- `app_id`: Tencent Cloud App ID (type: string)
- `secret_id`: Tencent Cloud Secret ID (type: string)
- `secret_key`: Tencent Cloud Secret Key (type: string)
- `codec`: Audio codec (type: string, default: "pcm")
- `emotion_category`: Emotion category (type: string, default: "")
- `emotion_intensity`: Emotion intensity (type: int64, default: 0)
- `enable_words`: Enable word-level timing (type: bool, default: false)
- `sample_rate`: Audio sample rate (type: int64, default: 24000)
- `speed`: Speech speed range [-2.0, 6.0] (type: float32, default: 0)
- `voice_type`: Voice type ID (type: string, default: "0")
- `volume`: Volume range [-10, 10] (type: float32, default: 0)
