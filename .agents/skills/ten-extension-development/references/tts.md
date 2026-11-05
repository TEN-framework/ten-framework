# TTS extension

The behavioral contract (transport model, flush, TTFB, dumps, guarder timing
hooks) lives in the `ten-tts-extension-development` skill. Load it and follow
it. This file only pins the package format so TTS packages look like the other
types.

## Templates

| Vendor transport | Copy from |
| ---------------- | --------- |
| HTTP (one request → audio body/chunks) | `rime_http_tts` or `mistral_tts_python` |
| WebSocket / SDK streaming | `deepgram_tts` |

## Package layout

```text
<vendor>_tts_python/
├── __init__.py
├── addon.py
├── config.py
├── extension.py
├── <vendor>_tts.py       # vendor client — not client.py
├── manifest.json         # imports tts-interface.json
├── property.json         # vendor options under "params"
├── pyproject.toml
├── requirements.txt
├── README.md
└── tests/
    ├── __init__.py
    ├── bin/start
    ├── conftest.py
    ├── configs/
    │   ├── property_basic_audio_setting1.json   # sample_rate A + valid key
    │   ├── property_basic_audio_setting2.json   # sample_rate B + valid key
    │   ├── property_dump.json                   # dump true
    │   ├── property_miss_required.json          # empty key
    │   └── property_invalid.json                # invalid key
    ├── test_basic.py             # event order, audio, duration
    ├── test_error_msg.py         # fatal vs non-fatal, vendor_info
    ├── test_metrics.py           # TTFB
    ├── test_params.py            # params pass-through, secret stripping
    ├── test_robustness.py        # empty/odd text, reconnect
    └── test_state_machine.py     # append, interleave, flush
```

These six test files are the set shared by `deepgram_tts`, `rime_http_tts`,
`mistral_tts_python`, `elevenlabs_tts2_python`, and `cartesia_tts`; copy them
from the template and adapt the mocks to the new client.

If the vendor outputs a single sample rate, both `basic_audio_setting` files
use it and the README states that the vendor supports one rate. If the vendor
has no auth, `property_miss_required.json` / `property_invalid.json` must
target the required field that does exist (for example an empty or malformed
`url`), and the README says so.

## Format pitfalls seen in practice

- Client module named `client.py` instead of `<vendor>_tts.py`.
- No `tests/bin/start` or `tests/configs/`, so neither `task test-extension`
  nor the guarder can run.
- `conftest.py` stubbing `ten_runtime` / `ten_ai_base` through `sys.modules`
  instead of the template `FakeApp` fixture.
- TTFB timer started before the WebSocket connect, so connection setup is
  counted as TTFB. Start it right before the first provider send.
- Per-request PCM dump writers never closed or removed.
- Cancellation flag reset per text segment, so remaining segments of a long
  request keep synthesizing after a flush.
- A private test-server IP used as the default endpoint in `property.json`
  and `.env.example`.
- Edits to `docker-compose.yml`, example `Taskfile.yml`, or unrelated
  extensions bundled into the extension change.
