# Cosy TTS Python Extension

A text-to-speech extension for the TEN Framework that integrates with the Cosy TTS service using the dashscope package.

## Overview

This extension provides high-quality text-to-speech synthesis using the Cosy TTS service through the official dashscope Python SDK. It follows the same architecture and patterns as other TTS extensions in the TEN Framework, ensuring consistency and maintainability.

## Configuration
Set the following environment variables:
- `COSY_TTS_API_KEY`: Your Cosy API Key

## Properties

### Top-level Properties
- `dump`: Enable audio dump for debugging (type: bool)
- `dump_path`: Path for audio dump files (type: string)

### TTS Parameters (nested under `params`)

### Optional Parameters
- `api_key`: Your Cosy TTS API key for authentication (dashscope API key)
- `model`: TTS model to use (default: "cosyvoice-v1")
- `sample_rate`: Audio sample rate in Hz (default: 16000)
- `voice`: Voice name for synthesis (default: "longxiaochun")
