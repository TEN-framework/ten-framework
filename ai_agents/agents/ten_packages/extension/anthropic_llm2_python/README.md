# anthropic_llm2_python

An Anthropic Claude LLM2 extension for the TEN framework, talking to the
Anthropic Messages API directly.

No extension previously targeted Claude. `bedrock_llm_python` is an Amazon
Nova extension that happens to use Bedrock's model-agnostic Converse API, so
overriding its `model` property could reach Claude, but that path is
undocumented and cannot express adaptive thinking or effort.

## Features

- Native Anthropic Messages API integration (no OpenAI-compatible shim)
- Full compatibility with the TEN LLM2 interface
- Streaming and non-streaming responses
- Tool calling support
- Reasoning surfaced through the LLM2 reasoning events, from Claude's native
  `thinking` blocks
- Vision support for both remote image URLs and inline base64 data URIs
- Effort-based cost/latency control
- Server-side fallback on safety refusals, so a voice agent never goes silent

## API

Refer to the `api` definition in [manifest.json](manifest.json) and default
values in [property.json](property.json).

| **Property** | **Type** | **Description** |
|---|---|---|
| `api_key` | `string` | API key for authenticating with Anthropic |
| `base_url` | `string` | Override the API base URL. Leave empty for the default |
| `model` | `string` | Model identifier (default `claude-opus-5`) |
| `max_tokens` | `int` | Maximum output tokens. Caps thinking **and** response text together |
| `prompt` | `string` | System prompt for the model |
| `proxy_url` | `string` | Optional HTTP proxy |
| `effort` | `string` | `low`, `medium`, `high`, `xhigh`, or `max`. Default `low` |
| `thinking_display` | `string` | `summarized` or `omitted`. Default `summarized` |
| `refusal_fallback` | `bool` | Route safety refusals to a fallback model. Default `true` |
| `refusal_message` | `string` | Spoken when a request is refused and no fallback succeeds |
| `custom_headers` | `object` | Extra HTTP headers. Scalar values only |

There is deliberately no `temperature`, `top_p`, `top_k`, `presence_penalty`,
`frequency_penalty`, or `seed`. Claude Opus 5 rejects the first three with a
400 and does not accept the rest; `effort` is the tuning knob in their place.

## Configuration

Set the `ANTHROPIC_API_KEY` environment variable with your Anthropic API key:

```bash
export ANTHROPIC_API_KEY=your_api_key
```

Optionally override the model or route through a proxy:

```bash
export ANTHROPIC_MODEL=claude-sonnet-5
export ANTHROPIC_PROXY_URL=http://127.0.0.1:7890
```

## Usage

Point a graph's LLM node at this addon. In any example's
`tenapp/property.json`, change the `addon` field of the LLM node:

```json
{
  "type": "extension",
  "name": "llm",
  "addon": "anthropic_llm2_python",
  "property": {
    "model": "claude-opus-5",
    "max_tokens": 2048,
    "effort": "low"
  }
}
```

No other change is needed. The extension implements the same
`llm-interface.json` contract as `openai_llm2_python`, so the surrounding
connections keep working unchanged.

## Model compatibility

Set `model` to any Claude model Anthropic currently serves. The extension
adjusts the request to what each one accepts, so switching models is a
one-line property change.

| Model | Reasoning events | `effort` | Refusal fallback |
|---|---|---|---|
| `claude-opus-5` (default) | yes | `low`–`max` | yes |
| `claude-fable-5` | yes | `low`–`max` | yes |
| `claude-sonnet-5` | yes | `low`–`max` | no |
| `claude-opus-4-8`, `claude-opus-4-7` | yes | `low`–`max` | no |
| `claude-opus-4-6`, `claude-sonnet-4-6` | yes | `xhigh`, `max` → `high` | no |
| `claude-haiku-4-5` | no | ignored | no |
| `claude-sonnet-4-5`, `claude-opus-4-5`, `claude-opus-4-1` | no | ignored | no |

Three separate capability gates are applied, because their support windows do
not line up:

- **Adaptive thinking and `effort`** exist on the current generation only.
  Older served models — Haiku 4.5, Sonnet 4.5, Opus 4.5 and earlier — reject
  them, so those requests are sent plain. Reasoning events are unavailable
  there; everything else, tools included, works normally.
- **Refusal fallback** is narrower still: only Opus 5 and Fable 5 accept the
  `fallbacks` parameter. Sonnet 5, Opus 4.8 and Opus 4.6 return
  `'<model>' does not support the 'fallbacks' parameter`, so it is withheld
  and refusals surface as `refusal_message` instead.
- **`effort` above `high`** arrived with Opus 4.7. On Opus 4.6 and Sonnet 4.6
  both `xhigh` and `max` are clamped to `high` rather than raising, so a
  misconfigured graph keeps talking.

The `model` property is the only thing consulted. A caller that sets
`LLMRequest.model` — `vision_analyze_tool_python` hardcodes `gpt-4o` — is
ignored rather than having that id forwarded to Anthropic, matching
`openai_llm2_python`.

A model newer than this extension gets the current-generation path by default,
so new releases work without a code change. Fallback is the exception — it is
withheld for unrecognised models, since sending it where it is unsupported
fails the whole request.

## Notes on the defaults

### `effort` defaults to `low`

The Anthropic API defaults to `high`. These graphs are realtime voice
assistants, where the latency of a single turn dominates the experience, and
`high` noticeably delays the first token. Claude Opus 5 performs unusually
well at `low`.

Raise it for text or agentic graphs — `xhigh` is the recommended setting for
coding and agentic work.

### `thinking_display` defaults to `summarized`

Claude emits reasoning as structured `thinking` blocks, which this extension
maps onto `MESSAGE_REASONING_DELTA` / `MESSAGE_REASONING_DONE`. The API default
for `display` is `omitted`, which still streams thinking blocks but with empty
text — so a reasoning UI would render blank with no error. Setting
`summarized` keeps it populated.

Set `omitted` to save tokens if you do not display reasoning.

### `max_tokens` defaults to 2048

On Claude Opus 5, `max_tokens` caps thinking plus response text together, and
thinking is on by default. The 512 used by some other LLM extensions would
truncate mid-answer.

### Refusals

Anthropic's safety classifiers can decline a request with HTTP 200,
`stop_reason: "refusal"`, and an empty content list. For a voice agent that is
silence, and the user cannot distinguish it from a network failure.

With `refusal_fallback` enabled the request is retried server-side on a
fallback model within the same call. If the whole chain still refuses, the
extension emits `refusal_message` so the agent says something. Set
`refusal_fallback` to `false` to stay off the beta endpoint entirely.

## Development

From the `ai_agents` directory:

```bash
task format
task check
task lint-extension EXTENSION=anthropic_llm2_python
```

## Tests

`tests/` runs the extension inside the TEN runtime rather than calling the
adapter directly, so it covers addon registration, the `llm-interface.json`
import, property injection, and the `chat_completion` cmd dispatch:

```bash
task test-extension \
  EXTENSION=agents/ten_packages/extension/anthropic_llm2_python
```

`test_extension_loads_and_dispatches` needs no credentials — it uses an
invalid key and asserts the failure comes back from the API, which can only
happen if the whole path worked. The remaining tests call Anthropic for
real and are skipped unless `ANTHROPIC_API_KEY` is set; they cover
streaming, reasoning events, the `thinking_display` toggle, a two-turn tool
call round trip, and the legacy-model path.

Set `ANTHROPIC_MODEL` to run them against a different model. The
reasoning tests skip themselves on models that cannot think.

Code style follows the framework conventions: 80-character lines, Black
formatting, type hints on all parameters and return types, and explicit
exception logging via `ten_env.log_*`.
