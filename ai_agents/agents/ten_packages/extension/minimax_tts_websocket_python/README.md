# Minimax TTS2 Python Extension

This extension provides Minimax Text-to-Speech (TTS) capabilities using the new AsyncTTS2BaseExtension framework.

## Features

- Streaming TTS synthesis using Minimax API
- Support for multiple voice types and models
- Configurable sample rates and audio formats
- TTFB (Time to First Byte) metrics
- Audio dump functionality for debugging
- Comprehensive error handling

## Configuration

Set the following environment variables:
- `MINIMAX_TTS_API_KEY`: Your Minimax API key
- `MINIMAX_TTS_GROUP_ID`: Your Minimax group ID

## Properties

- `api_key`: Minimax API key
- `group_id`: Minimax group ID
- `model`: TTS model (default: "speech-01-turbo")
- `voice_id`: Voice ID (default: "male-qn-qingse")
- `sample_rate`: Audio sample rate (default: 32000)
- `url`: API endpoint URL
- `request_timeout_seconds`: Request timeout
- `dump`: Enable audio dump for debugging
- `dump_path`: Path for audio dump files