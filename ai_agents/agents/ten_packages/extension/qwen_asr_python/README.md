# Qwen ASR Python

As of: 2026-11-26 07:11:35 UTC (+0000)
Codebase commit: `f8256f7aaf84ea3a5ed51608af1bfd67bb0ae39c`

TEN ASR extension for Qwen `qwen-audio-3.1-asr-flash-streaming` over the
DashScope duplex WebSocket API.

Use `${env:DASHSCOPE_API_KEY}` in `property.json`; do not put an API key in the
extension package. The default request uses 16 kHz PCM, `near_meeting_16k`, and
240 ms sentence silence, matching the Qwen admission report.

Finalize ends the current recognition task without resetting the base audio
timeline or cumulative send metrics. Each successfully started task records its
origin in the cumulative provider-audio timeline; sentence and word timestamps
are mapped through that origin. The same accounting is retained across reconnects.

Set `params.language` to the fallback language for ASR results, for example
`en-US`. Initialization merges known config fields from `params` using
`config.update(config.params)` before applying defaults and validating config.
`params.language` takes precedence over the top-level `language` property; the
top-level property remains supported when the nested value is absent.
Explicit `params.language_hints` take precedence over
hints derived from this language. Vendor-reported result languages take
precedence over the configured fallback.
