# RTZR ASR

Streaming speech recognition over RTZR WebSocket, using the TEN ASR interface.

## Configuration

Set `RTZR_CLIENT_ID` and `RTZR_CLIENT_SECRET` in your application's environment.
TEN resolves the `${env:...}` defaults in `property.json`; explicit graph node
properties override those defaults. The extension does not load `.env` files.

| Property in `params` | Environment default | Default |
| --- | --- | --- |
| `client_id` | `RTZR_CLIENT_ID` | Required |
| `client_secret` | `RTZR_CLIENT_SECRET` | Required |
| `api_base` | `RTZR_API_BASE` | `https://openapi.vito.ai` |
| `websocket_url` | `RTZR_WEBSOCKET_URL` | `wss://openapi.vito.ai` |
| `model_name` | — | `sommers_ko` |
| `sample_rate` | — | `16000` |
| `encoding` | — | `LINEAR16` |

Both URLs are **base URLs**, without `/v1/authenticate` or
`/v1/transcribe:streaming`. Separate HTTP and WebSocket endpoints are supported,
including `http`/`ws` for explicitly configured internal deployments.
Override both endpoints when using a different deployment. An explicitly empty
`websocket_url` derives its scheme and host from `api_base`.

Models `sommers_ko`, `sommers_ja`, and `sommers_en` report `ko-KR`, `ja-JP`, and
`en-US`, respectively. Input is mono signed PCM16; `sample_rate` can be configured
from 8000 to 48000 Hz and must match the incoming audio. The extension does not
resample audio. Additional RTZR recognition parameters such as `use_itn`,
`use_punctuation`, `epd_time`, and `keywords` can be supplied in `params`.
Credentials and base URLs are excluded from recognition query parameters.

Top-level properties:

- `dump` (default `false`) and `dump_path` (default `/tmp`): write successfully
  sent input to `rtzr_asr_in.pcm`. Use a distinct directory for each instance.
- `finalize_timeout` (default `5.0` seconds, maximum `60`): bounded wait for a
  final result. A pending hypothesis or a stream with no results reports an
  error on timeout. RTZR does not acknowledge Finalize separately after an
  automatic final followed by silence: when no hypothesis remains, local
  completion is accompanied by the vendor metric `finalize_without_result=1`.
  This is not an additional vendor final result or acknowledgement.

## Graph

Add the extension dependency to the application's `manifest.json` using the
appropriate relative path, for example from an example's `tenapp` directory:

```json
{"path": "../../../ten_packages/extension/rtzr_asr_python"}
```

Add an ASR node and route PCM audio to it and recognition results to your consumer:

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "asr",
      "addon": "rtzr_asr_python",
      "extension_group": "asr",
      "property": {"params": {"model_name": "sommers_ko"}}
    }
  ],
  "connections": [
    {
      "extension": "agora_rtc",
      "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "asr"}]}]
    },
    {
      "extension": "asr",
      "data": [{"name": "asr_result", "dest": [{"extension": "main"}]}]
    }
  ]
}
```

This fragment assumes existing `agora_rtc` and `main` nodes. Route
`asr_finalize` data from your turn controller to `asr`, and
`asr_finalize_end`, `error`, `metrics`, and `connection_status_changed` back to
the controller as needed. Finalize preserves the WebSocket for the next turn;
EOS is only sent during connection shutdown.

Transient failures retry up to five times with 300 ms exponential backoff.
Authentication/configuration failures terminate immediately. Unsent audio is
retained up to 10 MiB; overflow discards oldest frames and emits an error.
Already accepted frames are not replayed after a disconnect, since RTZR has no
resume/acknowledgement protocol for individual audio frames.

## Tests

From `ai_agents`, after installing Python requirements and TEN tooling:

```bash
task test-extension EXTENSION=agents/ten_packages/extension/rtzr_asr_python
task asr-guarder-test EXTENSION=rtzr_asr_python -- -k 'not test_multi_language and not test_long_duration_stream'
```

Standalone tests use mock transport with the real TEN runtime and require no
credentials or external audio. They cover model/language mapping, interim/final
results, word timing, Finalize ordering, buffering, reconnection, and cleanup.

The common guarder uses real RTZR credentials from the application's environment.
Its Chinese-specific multilingual case is not applicable to these three Sommers
models. The command above excludes that case and the optional long-duration test.
To run the common long-duration test as well, omit `not test_long_duration_stream`
and the preceding `and` from the filter.

## References

- [RTZR streaming API](https://developers.rtzr.ai/docs/stt-streaming/)
- [TEN ASR extension guide](https://docs.agora.io/en/ai/reference/ten-agent/create-asr-extension)
