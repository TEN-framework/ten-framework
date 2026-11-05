# Shared package format (all types)

Source of truth: `docs/ai/L1/04_conventions.md`, `docs/ai/L1/05_workflows.md`,
and the template extensions named in each type reference. When this file and a
recent template disagree, follow the template and note it.

## Naming

| Item | Pattern | Example |
| ---- | ------- | ------- |
| Directory / addon / manifest name | `<vendor>_<type>_python` | `xai_asr_python`, `anthropic_llm2_python` |
| LLM directory | `<vendor>_llm2_python` | `gemini_llm2_python` |
| Config class | `<Vendor><Type>Config` | `XAIASRConfig` |
| Extension class | `<Vendor><Type>Extension` | `XAIASRExtension` |
| Addon class | `<Vendor><Type>ExtensionAddon` | `XAIASRExtensionAddon` |
| Env vars | `<VENDOR>_<TYPE>_<FIELD>` or the vendor's usual name | `XAI_API_KEY`, `BLAZE_TTS_API_KEY` |
| Endpoint param | `base_url` (29 extensions use it; avoid `api_url`/`url`) | `"base_url": "wss://api.x.ai/v1/stt"` |

## Files every extension has

```text
<name>/
├── __init__.py          # license header + `from . import addon`
├── addon.py             # @register_addon_as_extension("<name>")
├── extension.py         # TEN lifecycle; subclass of the type base class
├── config.py            # Pydantic config (LLM templates may keep it in the client module)
├── <vendor client>.py   # name fixed per type reference
├── manifest.json
├── property.json
├── pyproject.toml
├── requirements.txt
├── README.md
└── tests/
    ├── __init__.py      # empty
    ├── bin/start        # copied verbatim from the template, executable
    ├── conftest.py      # FakeApp session fixture (copied from template)
    ├── configs/         # per-type required property_*.json files
    └── test_*.py
```

Type-specific additions (ASR `const.py`, `reconnect_manager.py`,
`tests/mock.py`) are listed in the type reference.

## Fixed file contents

`__init__.py`:

```python
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from . import addon
```

`addon.py` (import the extension lazily inside `on_create_instance` or at
module top — follow the template you copied):

```python
from ten_runtime import Addon, register_addon_as_extension, TenEnv


@register_addon_as_extension("<name>")
class <Vendor><Type>ExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        from .extension import <Vendor><Type>Extension

        ten_env.log_info("on_create_instance")
        ten_env.on_create_instance_done(<Vendor><Type>Extension(name), context)
```

`manifest.json` skeleton:

```json
{
  "type": "extension",
  "name": "<name>",
  "version": "0.1.0",
  "dependencies": [
    {"type": "system", "name": "ten_runtime_python", "version": "0.11"},
    {"type": "system", "name": "ten_ai_base", "version": "0.7"}
  ],
  "package": {
    "include": ["manifest.json", "property.json", "**.py", "README.md",
                "pyproject.toml", "requirements.txt"]
  },
  "api": {
    "interface": [
      {"import_uri": "../../system/ten_ai_base/api/<asr|tts|llm>-interface.json"}
    ],
    "property": {"properties": { "...": "every key that property.json can set" }}
  }
}
```

Every key in `property.json` (including nested `params` keys) must be declared
in `api.property`. Use TEN types: `string`, `bool`, `int32`, `int64`,
`float64`, `object`.

`pyproject.toml`:

```toml
[project]
name = "<name-with-hyphens>"
version = "0.1.0"
requires-python = ">=3.10"
dependencies = ["<same pins as requirements.txt>"]
```

`requirements.txt` lists the same runtime dependencies. Do not add test-only
packages (`pytest-asyncio` etc.) — tests use `asyncio.run(...)` or
`AsyncExtensionTester`, which the container already provides.

`tests/bin/start` (copy verbatim; `chmod +x`):

```bash
#!/bin/bash
set -e
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
export PYTHONPATH=.ten/app:.ten/app/ten_packages/system/ten_runtime_python/lib:.ten/app/ten_packages/system/ten_runtime_python/interface:.ten/app/ten_packages/system/ten_ai_base/interface:$PYTHONPATH
pytest -s tests/ "$@"
```

`tests/conftest.py`: copy the template's `FakeApp` / `global_setup_and_teardown`
session fixture unchanged. Tests import the package by its directory name
(`from <name>.extension import ...`). Do **not** replace the TEN runtime with
`sys.modules` stubs — tests must run through `task test-extension`.

## property.json and environment variables

| Syntax | Meaning |
| ------ | ------- |
| `${env:VAR}` | required; startup error if unset |
| `${env:VAR\|}` | optional; empty string |
| `${env:VAR\|default}` | optional with default |

- Secrets only through env vars. Defaults may be public vendor URLs only —
  never a private/test IP, personal tunnel, or signed URL.
- ASR and TTS keep vendor options in a `params` object; LLM templates use flat
  top-level keys (follow the LLM template).
- Tests under `tests/configs/` use the same syntax.

## Config model rules

- Parse with `Model.model_validate_json(config_json)` from
  `await ten_env.get_property_to_json("")`.
- `validate_config()` raises `ValueError` for missing credentials (unless the
  vendor truly has no auth — say so in the README), bad URLs, unsupported
  sample rates/encodings/languages.
- Redact any key containing `key`, `token`, `secret`, `signature`, `password`
  before logging (`ten_ai_base.utils.encrypt`).
- Log exactly once: `ten_env.log_info(f"config: {redacted}",
  category=LOG_CATEGORY_KEY_POINT)` — QA matches the `config:` prefix.
- Never log an API key, Authorization header, or a URL that carries a token.

## Error classification

| Situation | Code |
| --------- | ---- |
| Invalid/missing config, 401/403, bad credentials | `ModuleErrorCode.FATAL_ERROR` (-1000) |
| Retry ceiling reached | `FATAL_ERROR` |
| Transient disconnect, timeout, 5xx, single bad request | `ModuleErrorCode.NON_FATAL_ERROR` (1000) |

Vendor-originated errors carry
`ModuleErrorVendorInfo(vendor=self.vendor(), code=str(code), message=msg)`.
`vendor_info.code` is always a string.

## Logging categories

`LOG_CATEGORY_KEY_POINT` for lifecycle/config, `LOG_CATEGORY_VENDOR` for
vendor status (`vendor_status_changed: ...`, `vendor_error: ...`),
`LOG_CATEGORY_TRANSCRIPTS` for recognized or synthesized text.

## README.md

Sections: overview, features, configuration table (property, type, default,
description), environment variables, protocol summary (the contract from
SKILL.md step 2), example graph node, how to run tests.

## Formatting and lint

Black `--line-length 80`; pylint warnings fail CI (unused imports included).
Run `task format && task check && task lint` from `/app` in the container.

## Final checklist

- [ ] Four names equal: directory, addon decorator, manifest `name`, graph `addon`.
- [ ] `__init__.py` imports `addon`; license header on every `.py`.
- [ ] Vendor client module uses the type's fixed file name.
- [ ] Every `property.json` key declared in `manifest.json` `api.property`.
- [ ] `requirements.txt` and `pyproject.toml` agree; no test-only deps.
- [ ] `tests/bin/start` copied verbatim and executable; `tests/__init__.py` exists.
- [ ] `tests/conftest.py` uses the TEN `FakeApp` fixture, not import stubs.
- [ ] All required `tests/configs/property_*.json` for the type exist.
- [ ] No secret, private IP, or signed URL in any committed file.
- [ ] Config logged once with `config:` prefix and redaction.
- [ ] Fatal vs non-fatal classification and `vendor_info` on vendor errors.
- [ ] Nothing outside the package changed unless the user asked.
- [ ] `task format`, `task check`, `task lint`, `task test-extension`, and the
      guarder (ASR/TTS) were run, or the reason they could not run is reported.
