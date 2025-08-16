# TTS Guarder Test Guide

This document describes how to run Guarder Test for TTS

## Environment Variables

Before running the test, you need to set the following environment variables:

```bash
# TTS Vendor Services API Key
export VENDOR_TTS_API_KEY=your_api_key_here
#for example:
export ELEVENLABS_TTS_API_KEY=your_elevenlabs_api_key

```

Or create a `.env` file in the project root:

```bash
# .env file
ELEVENLABS_TTS_API_KEY=your_elevenlabs_api_key
```

## Test Text

prepare mutiple text for testing different scenario

## Running the Test

```bash
# Run the test
bash tests/bin/start tests/test_elevenlabs_tts_basic.py::test_short_text --extension_name=elevenlbas_tts_python
```