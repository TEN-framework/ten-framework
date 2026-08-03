# Cosy TTS Python Extension

A text-to-speech extension for the TEN Framework that integrates with the Cosy TTS service using the dashscope package.

## Overview

This repository-owned extension provides text-to-speech synthesis using the
official DashScope Python SDK. It uses the SDK object pool so worker processes
preconnect WebSockets and reuse them across successful or cancelled tasks.

## Configuration
Set the following environment variables:
- `COSY_TTS_API_KEY`: Your Cosy API Key
- `COSY_TTS_URL`: Optional DashScope WebSocket URL
- `COSY_TTS_WORKSPACE_ID`: Optional DashScope workspace ID

## Properties

### Top-level Properties
- `dump`: Enable audio dump for debugging (type: bool)
- `dump_path`: Path for audio dump files (type: string)

### TTS Parameters (nested under `params`)

### Optional Parameters
- `api_key`: Your Cosy TTS API key for authentication (dashscope API key)
- `model`: TTS model to use (default: "cosyvoice-v3-flash")
- `sample_rate`: Audio sample rate in Hz (default: 16000)
- `voice`: Voice name for synthesis (default: "longanyang")
- `url`: Optional full DashScope WebSocket URL. When omitted, the DashScope
  SDK default is used.
- `pool_size`: Number of preconnected synthesizers shared by all Cosy
  extension instances in one worker process (default: 1, valid range: 1-100).
  For production, size the pool to roughly 1.5-2 times peak concurrency without
  exceeding the account QPS limit.
- `headers`: Optional custom WebSocket handshake headers.
- `pool_wait_timeout_ms`: Maximum wait for a pooled synthesizer (default: 1000).
- `first_audio_timeout_ms`: Maximum wait for first audio (default: 5000).
- `task_timeout_ms`: Maximum lifetime of one synthesis task (default: 30000).
- `input_idle_timeout_ms`: Maximum interval between streaming text chunks
  (default: 20000; must be below the provider's 23-second limit).
- `cancel_timeout_ms`: Maximum wait for cancellation completion (default: 1000).
- Other keys under `params` are passed unchanged to DashScope task parameters.
  Extension-owned keys and `format` are excluded. This extension always emits
  mono 16-bit PCM.

`enable_ssml=true` is rejected because CosyVoice SSML only permits one text
chunk, while this extension intentionally preserves streaming multi-chunk input.
Word-level timestamp output is not currently supported.

## Connection reuse constraints

- A synthesizer is returned only after DashScope reports `task-finished`.
- Failed connections are closed and returned to the SDK pool for replacement.
- DashScope authenticates pooled WebSockets during the initial handshake.
  Therefore one worker process must use the same API key, URL, and pool size
  for all Cosy extension instances.

## TTFB comparison scripts

The scripts under `scripts/` compare the public WebSocket protocol with the
DashScope SDK while keeping model, voice, text, sample rate, and endpoint the
same. Use a complete sentence ending in punctuation so CosyVoice starts
synthesis without waiting for more text.

Set credentials and the workspace-specific endpoint:

```bash
export COSY_TTS_API_KEY='your-api-key'
export COSY_TTS_URL='wss://your-workspace-id.cn-beijing.maas.aliyuncs.com/api-ws/v1/inference'
```

Measure a reused direct WebSocket connection:

```bash
uv run --no-project --with websocket-client \
  python scripts/benchmark_websocket_ttfb.py --warmup 3 --iterations 20
```

Measure the pooled SDK path used by this extension:

```bash
uv run --no-project --with 'dashscope==1.26.4' \
  python scripts/benchmark_sdk_ttfb.py \
  --mode pooled --pool-size 1 --warmup 3 --iterations 20
```

For a cold-connection comparison, add `--fresh-connection-per-task` to the
direct script and use `--mode fresh` for the SDK script. Compare:

- `task_ttfb_ms` for reused-connection protocol overhead.
- `end_to_end_ttfb_ms` for cold connections, including connection/object setup.
- `task_start_ack_ms` to distinguish task admission delay from synthesis delay.
- `sdk_reported_ttfb_ms` against the independently measured SDK callback value.

Each task and the final summary are emitted as one JSON object per line. API
keys and text payloads are never printed.
