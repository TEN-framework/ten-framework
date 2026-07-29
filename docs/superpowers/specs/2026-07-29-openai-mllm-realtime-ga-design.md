# openai_mllm_python: Realtime beta → GA migration

Date: 2026-07-29
Status: approved

## Problem

`openai_mllm_python` speaks the deprecated beta shape of the OpenAI Realtime
API on three axes:

1. `realtime/connection.py:80` sends the `OpenAI-Beta: realtime=v1` header,
   which the GA interface requires callers to drop.
2. `realtime/struct.py:273-278` recognises only beta server event names
   (`response.audio.delta`, `response.text.delta`,
   `response.audio_transcript.delta`). GA renamed these to
   `response.output_audio.delta`, `response.output_text.delta`,
   `response.output_audio_transcript.delta`.
3. `session.update` is serialised in the flat beta shape. GA requires
   `session.type: "realtime"`, audio configuration nested under
   `session.audio.input` / `session.audio.output`, `output_modalities`
   instead of `modalities`, and `max_output_tokens` instead of
   `max_response_output_tokens`.

Two model-configuration defects compound this:

- `extension.py:76` defaults `model` to `gpt-4o`, which is not a realtime
  model. Any deployment that does not override the property in
  `property.json` fails to connect.
- `property.json` pins `gpt-realtime` (August 2025). The current recommended
  model is `gpt-realtime-2.1`, which exposes a `reasoning.effort` control that
  the extension has no way to set.

The extension has no tests at all, so none of the above is caught.

## Scope

In scope: `openai_mllm_python` only.

Explicitly out of scope: `azure_mllm_python`, `glm_mllm_python`,
`stepfun_mllm_python`. Those carry their own copies of `realtime/` but target
independent endpoints (`AZURE_AI_FOUNDRY_BASE_URI` with its own `api_version`,
`wss://open.bigmodel.cn`, `wss://api.stepfun.com`) and none of them sends the
beta header. They cloned OpenAI's beta *event schema*, but they are separate
services on their own release schedules — propagating GA event names to them
would break them.

## Design

### Wire boundary

Internal dataclass and enum *names* stay as they are. The beta→GA difference is
confined to a serialisation boundary inside `realtime/struct.py`, which imports
only the standard library (`json`, `dataclasses`, `typing`, `enum`, `uuid`) and
therefore stays unit-testable without the TEN runtime.

**Inbound.** The `EventType` enum values change to the GA strings. The
dispatch at `struct.py:842+` compares `data["type"]` against those enum
members, so it follows automatically. `extension.py:165-432` matches on
dataclass *classes*, not strings, and does not change.

**Outbound.** A `session_update_to_ga_dict()` mapper converts the flat internal
`SessionUpdateParams` into the GA nested payload. `to_json()` dispatches to it
for `SessionUpdate` messages, so `connection.py`'s `send_request(su)` path is
untouched.

### GA session.update payload

```json
{
  "type": "session.update",
  "session": {
    "type": "realtime",
    "model": "gpt-realtime-2.1",
    "instructions": "...",
    "output_modalities": ["audio"],
    "max_output_tokens": 2048,
    "audio": {
      "input": {
        "turn_detection": {"type": "semantic_vad", "eagerness": "auto"},
        "transcription": {"language": "en"}
      },
      "output": {"voice": "alloy"}
    },
    "tools": [],
    "tool_choice": "auto",
    "reasoning": {"effort": "low"}
  }
}
```

Field mapping:

| internal (beta)              | GA                                  |
| ---------------------------- | ----------------------------------- |
| `modalities`                 | `output_modalities`                 |
| `voice`                      | `audio.output.voice`                |
| `turn_detection`             | `audio.input.turn_detection`        |
| `input_audio_transcription`  | `audio.input.transcription`         |
| `input_audio_format`         | `audio.input.format`                |
| `output_audio_format`        | `audio.output.format`               |
| `max_response_output_tokens` | `max_output_tokens`                 |
| (new) `reasoning_effort`     | `reasoning.effort`                  |

`instructions`, `model`, `tools` and `tool_choice` stay at the top level of
`session`.

### Audio format

`audio.input.format` and `audio.output.format` take an **object**
(`{"type": "audio/pcm", "rate": 24000}`), not the string `"pcm16"` the
published reference still shows. The GA implementation rejects the string form
with `Invalid type for 'session.audio.input.format': expected an object, but
got a string instead`.

`extension.py` never sets these fields today, and `to_json()` drops `None`
values, so the extension currently sends no format at all and inherits the GA
default of PCM16 24 kHz mono — which matches its own `sample_rate: 24000`. The
mapper preserves that: it omits `format` when the internal field is `None`, and
emits the object shape when it is set.

### Model and reasoning configuration

- `extension.py` config default `model` becomes `gpt-realtime-2.1`, replacing
  the non-realtime `gpt-4o`.
- `property.json` `model` becomes `gpt-realtime-2.1`.
- New optional property `reasoning_effort`, accepted values `minimal`, `low`,
  `medium`, `high`, `xhigh`, plus empty string meaning "omit the field". The
  default is empty so behaviour does not change for existing deployments that
  do not opt in.
- `manifest.json` gains the `reasoning_effort` property and a version bump.

## Testing

New `tests/` directory, which `Taskfile.yml:75` auto-discovers via
`find agents/ten_packages/extension -type d -exec test -d "{}/tests" \; -print`,
so the suite joins `task test` with no wiring change. `tests/bin/start` follows
the existing convention: set `PYTHONPATH` to the standalone `.ten/app` tree,
then `pytest -s tests/`.

Two deliberate deviations from how other extensions lay out `tests/`:

- **No `tests/__init__.py`, plus an empty `tests/pytest.ini`.** Other
  extensions make `tests` a subpackage, which means pytest imports
  `openai_mllm_python/__init__.py` — and that imports `addon`, which imports
  the native `ten_runtime` module. Those suites therefore only run where a
  runtime has been installed. Dropping `__init__.py` and anchoring pytest's
  rootdir at `tests/` keeps the extension package out of the collection tree,
  so this suite runs anywhere. That matters because the whole design puts the
  wire format in a layer that depends on nothing.
- **`tests/test_connection.py` reads `connection.py` via `ast` instead of
  importing it.** `connection.py` needs `ten_runtime` for a type hint, and the
  invariants worth asserting (no beta header, realtime default model) are
  visible in the source. Inspecting string *constants* rather than raw text
  means comments explaining the migration cannot affect the result.

Formatting follows `black --line-length 80`, which is what the existing files
in this extension already satisfy. The repository has no committed black
config; 79 and 88 both report drift against unmodified files, 80 reports none.

Tests target the wire boundary, which needs no credentials, no network and no
TEN runtime:

1. **Inbound event names** — every GA server event string parses to the
   expected dataclass, and the renamed trio (`response.output_audio.delta`,
   `response.output_text.delta`, `response.output_audio_transcript.delta`)
   is covered explicitly.
2. **Outbound session.update** — the mapper emits `session.type: "realtime"`,
   nests voice/turn detection/transcription correctly, renames
   `modalities` → `output_modalities`, and omits `audio.*.format` when unset.
3. **Audio format object shape** — when a format is set, it serialises to
   `{"type": "audio/pcm", "rate": N}` and never to a bare string.
4. **Reasoning effort** — emitted as `reasoning.effort` when configured, and
   the `reasoning` key is absent entirely when not.
5. **Config defaults** — the default model is a realtime model, guarding
   against a regression to `gpt-4o`.
6. **No beta residue** — an assertion that no beta event string and no
   `OpenAI-Beta` header survives in the module.

## Relationship to PR #2167

PR #2167 performs the same protocol migration (`+82/-12` in `struct.py`) using
the same mapper approach. It is `MERGEABLE` but `BLOCKED`, with zero reviews
and zero comments since 2026-05-21. This work supersedes it by adding the model
defaults, the `reasoning_effort` control, and the test suite the extension has
never had. The PR description will state the overlap and defer to the
maintainers on which to take.
