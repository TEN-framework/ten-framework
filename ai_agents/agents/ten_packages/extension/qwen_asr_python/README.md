# Qwen ASR Python

TEN ASR extension for Qwen `qwen-audio-3.1-asr-flash-streaming` over the
DashScope duplex WebSocket API.

Use `${env:DASHSCOPE_API_KEY}` in `property.json`; do not put an API key in the
extension package. The default request uses 16 kHz PCM, `near_meeting_16k`, and
240 ms sentence silence, matching the Qwen admission report.
