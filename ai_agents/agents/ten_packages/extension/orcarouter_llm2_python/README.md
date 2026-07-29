# orcarouter_llm2_python

An extension for integrating [OrcaRouter](https://www.orcarouter.ai) into your
application. OrcaRouter is an OpenAI-compatible meta-router: a single endpoint
that fronts many upstream models and adds server-side routing plus a
per-request model-fallback chain. Because it speaks the OpenAI Chat Completions
protocol, this extension reuses the OpenAI SDK and only changes the defaults.

> Disclosure: I'm an engineer on the OrcaRouter team.

## Features

- OpenAI-compatible: chat completions, streaming, tool/function calling, and
  reasoning (`<think>` blocks and `reasoning_content`) all work unchanged.
- Automatic routing: point `model` at `orcarouter/auto` and OrcaRouter picks a
  live model per request (the routing strategy — cheapest / quality / balanced
  / adaptive — is configured in the OrcaRouter console).
- Model-fallback chain: pass `models` + `route: "fallback"` through request
  parameters to try several models in order until one succeeds.
- Attribution: `custom_headers` forwards `HTTP-Referer` / `X-Title` so the
  OrcaRouter console can report which client is calling.

## Configuration

Set your OrcaRouter API key (keys start with `sk-orca-`):

```bash
export ORCAROUTER_API_KEY="sk-orca-..."
# optional; defaults to orcarouter/auto
export ORCAROUTER_MODEL="orcarouter/auto"
```

Example model IDs: `orcarouter/auto` (per-request routing), `orcarouter/fusion`
(a Fusion panel), or any provider-prefixed model such as `openai/gpt-4o`,
`anthropic/claude-haiku-4.5`, `google/gemini-2.5-pro`. See the full catalog at
https://www.orcarouter.ai/models.

### Model-fallback chain

OrcaRouter tries each model in order until one succeeds (up to 5). Pass the
controls through the request `parameters`:

```json
{
  "models": ["openai/gpt-4o", "anthropic/claude-haiku-4.5"],
  "route": "fallback"
}
```

## API

Refer to the `api` definition in [manifest.json] and default values in
[property.json](property.json).

| **Property**     | **Type**   | **Description**                                                        |
|------------------|------------|------------------------------------------------------------------------|
| `api_key`        | `string`   | OrcaRouter API key (`sk-orca-...`), sent as a Bearer token             |
| `base_url`       | `string`   | API base URL (default `https://api.orcarouter.ai/v1`)                  |
| `model`          | `string`   | Model / router ID (default `orcarouter/auto`)                          |
| `prompt`         | `string`   | Default system prompt                                                   |
| `proxy_url`      | `string`   | Optional HTTP(S) proxy URL                                             |
| `custom_headers` | `object`   | Extra request headers (e.g. `HTTP-Referer`, `X-Title` for attribution) |

### Data In / Data Out / Command In / Command Out

Same as the other LLM2 extensions (`text_data` in/out, `flush` command),
provided by the shared `llm-interface.json` contract.
