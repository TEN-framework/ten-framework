---
name: ten-extension-development
description: Scaffold and implement a new TEN Framework ASR/STT, TTS, or LLM vendor extension under ai_agents/agents/ten_packages/extension/ that follows the repository's exact package format (file names, addon registration, manifest/property layout, config model, client module, tests/bin/start, tests/configs, guarder readiness). Use when the user asks to create, add, port, or "make an extension for" a speech-to-text, text-to-speech, or language-model provider, including from an endpoint URL, WebSocket protocol, HTTP API, or SDK. For TTS, also load ten-tts-extension-development. Do not use for reviewing an existing extension, for graph-only edits, or for non-AI extensions such as tools, avatars, or transports.
---

# Build a TEN ASR / TTS / LLM extension in the repository format

As of: 2026-11-05
Codebase commit: `64c08a63975b867f897481904d8c3df10367b313`

Every vendor is different — endpoint, auth, parameters, and wire messages —
but every extension of one type has the **same package shape, the same script
names, the same lifecycle flow, and the same test harness**. Your job is to keep
the shape identical and put all vendor differences in the config model and the
vendor client module.

## 1. Classify the request

| User says | Type | Base class | Reference |
| --------- | ---- | ---------- | --------- |
| ASR, STT, speech recognition, transcription | ASR | `AsyncASRBaseExtension` | [asr.md](references/asr.md) |
| TTS, speech synthesis, voice | TTS | `AsyncTTS2BaseExtension` (WS/SDK) or `AsyncTTS2HttpExtension` (HTTP) | [tts.md](references/tts.md) + the `ten-tts-extension-development` skill |
| LLM, chat completion, language model | LLM | `AsyncLLM2BaseExtension` | [llm.md](references/llm.md) |

"STT" and "ASR" are the same type in this repo. Name new speech-to-text
packages `<vendor>_asr_python` unless the user names the directory explicitly.

Always read [shared-format.md](references/shared-format.md) first, then the one
type reference. Load `docs/ai/L1/L2/asr_plugin_design_review.md` for ASR and
`docs/ai/L1/L2/extension_development.md` when you need payload details.

## 2. Pin down the vendor contract before writing code

Collect these facts and write them down (in the README "Protocol" section and
in your reply). If any are missing, ask once, in one message, before coding:

- Transport: HTTP request/response, HTTP chunked stream, WebSocket, or SDK.
- Endpoint URL(s) and how they vary by region/model.
- Auth: header, query token, or none. Which env var holds it.
- Request shape: first message/body, per-chunk messages, end/finalize message.
- Response shape: audio/text frames, terminal event, error event, IDs.
- Audio format in or out: encoding, sample rate(s), channels, sample width.
- Connection lifetime: one request per connection, or long-lived/multiplexed.
- Languages, voices, or models the vendor supports.

Never invent a protocol. If the user gives only a URL, inspect any source they
shared (scripts, `.env` files, server code, docs) for the contract; if it is
still unknown, stop and ask. A missing ASR contract must not be replaced by a
different vendor silently.

## 3. Copy a strong template, then rename

Start from the template for the matching type and transport (see the type
reference). Copy it, then rename **all four** identifiers so they are equal:

1. directory name
2. `@register_addon_as_extension("<name>")` in `addon.py`
3. `"name"` in `manifest.json`
4. `"addon"` in any graph node that uses it

Delete vendor logic you do not need instead of leaving dead branches. Keep the
file names, the license header, `from . import addon` in `__init__.py`, and the
`tests/bin/start` script exactly as the template has them.

## 4. Implement inside the fixed shape

- `config.py` — Pydantic model; vendor options in `params`; framework options
  (`dump`, `dump_path`, timeouts) as first-class fields; `validate_config()`;
  redacted `to_str()`/`to_json()`; log once with the `config:` prefix.
- Vendor client module — owns connection, auth, encoding/decoding, vendor
  errors, and cancellation. Named per the type reference (`recognition.py`,
  `<vendor>_tts.py`, `<vendor>_llm.py`).
- `extension.py` — owns TEN lifecycle, request/session IDs, metadata, events,
  metrics, error classification, and dumps. No raw protocol parsing here.

## 5. Scope rules

- Generate the extension package and its tests. Touch nothing else unless the
  user asks: no graph edits, no example apps, no `docker-compose.yml`,
  `Taskfile.yml`, `.env.example`, or fixes to unrelated extensions.
- Exception for TTS: `ten-tts-extension-development` requires an independent
  TTFB timing hook for every new provider. Add it in
  `ai_agents/agents/integration_tests/tts_guarder/tests/` (an observer under
  `ttfb_hooks/` or `provider_ttfb_hooks/`, its registry entry, and its tests)
  without waiting to be asked. Change nothing else in the guarder.
- If the user asks for a runnable graph, add a node to an existing example's
  `tenapp/property.json` and a `path` dependency in its `tenapp/manifest.json`
  **after** the package tests pass. Create a new example directory only if the
  user asks for a separate example.
- Never commit vendor secrets, private IPs as public defaults, or generated
  files (`manifest-lock.json`, `.ten/`, `bin/`, `out/`).
- Do not change framework base classes in `ten_ai_base`.

## 6. Verify, then report

Run, in the `ten_agent_dev` container, sequentially:

```bash
cd /app && task format && task check && task lint
cd /app && task test-extension EXTENSION=agents/ten_packages/extension/<name>
cd /app && task asr-guarder-test EXTENSION=<name> CONFIG_DIR=tests/configs   # ASR
cd /app && task tts-guarder-test EXTENSION=<name> CONFIG_DIR=tests/configs   # TTS
```

LLM extensions have no guarder; their standalone tests must drive the addon
through `AsyncExtensionTester` (see [llm.md](references/llm.md)).

Before reporting, walk the final checklist in
[shared-format.md](references/shared-format.md#final-checklist). Report:
files created, the vendor contract you implemented, which commands ran and
their results, which could not run and why (no Docker, no key), and every
place you diverged from the template. Say "validated with mock; real-vendor
guarder pending" when no real credential was available.
