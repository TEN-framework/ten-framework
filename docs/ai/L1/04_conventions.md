# 04 Conventions

> Coding patterns, naming, configuration, and formatting standards.

## Naming Conventions

| Item            | Pattern                                | Example                    |
| --------------- | -------------------------------------- | -------------------------- |
| Extension dir   | `<vendor>_<type>_python`               | `deepgram_asr_python`      |
| Addon name      | Same as directory name                 | `deepgram_asr_python`      |
| Example dir     | `voice-assistant-<variant>`            | `voice-assistant-realtime` |
| Config class    | `<Vendor><Type>Config(BaseModel)`      | `DeepgramTTSConfig`        |
| Client class    | `<Vendor><Type>Client`                 | `DeepgramTTSClient`        |

## Addon Registration

Every extension must register via decorator in `addon.py`:

```python
from ten_runtime import Addon, register_addon_as_extension, TenEnv

@register_addon_as_extension("deepgram_asr_python")
class DeepgramASRExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, name: str, context) -> None:
        ten.on_create_instance_done(DeepgramASRExtension(name), context)
```

The decorator name **must match** the `addon` field in `property.json` graph nodes.

## Base Class Selection

| Need                    | Base Class                    | Key Methods You Implement Most Often                          |
| ----------------------- | ----------------------------- | ------------------------------------------------------------- |
| Speech-to-text          | `AsyncASRBaseExtension`       | `vendor()`, `start_connection()`, `stop_connection()`, `send_audio()`, `finalize()`, `is_connected()`, `input_audio_sample_rate()`, `buffer_strategy()` |
| Text-to-speech (HTTP)   | `AsyncTTS2HttpExtension`      | `create_config()`, `create_client()`, `vendor()`, `synthesize_audio_sample_rate()` |
| Text-to-speech (WS)     | `AsyncTTS2BaseExtension`      | `vendor()`, `request_tts()`, `synthesize_audio_sample_rate()` |
| Chat completion         | `AsyncLLMBaseExtension`       | `on_call_chat_completion()`, `on_data_chat_completion()` |
| LLM function tool       | `AsyncLLMToolBaseExtension`   | `get_tool_metadata()`, `run_tool()`   |
| Generic / custom        | `AsyncExtension`              | `on_cmd()`, `on_data()`, etc.         |

## Pydantic Configuration

Extensions use Pydantic models for config validation:

```python
from pydantic import BaseModel, Field

class DeepgramTTSConfig(BaseModel):
    params: dict[str, Any] = Field(default_factory=dict)
```

Config is loaded from property.json in `on_init()`:
```python
config_json, _ = await ten_env.get_property_to_json("")
self.config = DeepgramTTSConfig(**json.loads(config_json))
```

## Environment Variable Syntax

In `property.json`, reference env vars:

| Syntax                | Behavior                                |
| --------------------- | --------------------------------------- |
| `${env:VAR_NAME}`     | Required — error if missing             |
| `${env:VAR_NAME\|}`   | Optional — empty string if missing      |
| `${env:VAR_NAME\|default}` | Optional — uses default if missing |

```json
{"params": {"api_key": "${env:DEEPGRAM_API_KEY}", "region": "${env:AZURE_REGION|}"}}
```

## Params Dict Pattern

Extensions using HTTP/WebSocket services store all config in a `params` dictionary:

1. **Store** `api_key` inside `params` dict in property.json and config
2. **Extract** for authentication headers in the client constructor
3. **Keep** convenience fields if needed, but preserve vendor params in config
4. **Strip** secrets only when creating the HTTP payload or WS query

```python
# In client constructor — extract for auth
self.api_key = config.params.get("api_key", "")
self.headers = {"Authorization": f"Bearer {self.api_key}"}

# In request method — strip before sending
vendor_params = {**self.config.params}
vendor_params.pop("api_key", None)
```

## Sensitive Data Logging

Implement `to_str()` to encrypt sensitive fields before logging:

```python
def to_str(self, sensitive_handling: bool = True) -> str:
    if not sensitive_handling:
        return f"{self}"
    config = copy.deepcopy(self)
    if config.params and "api_key" in config.params:
        config.params["api_key"] = utils.encrypt(config.params["api_key"])
    return f"{config}"
```

## Logging

- Use `ten_env.log_info()`, `ten_env.log_warn()`, `ten_env.log_error()`, `ten_env.log_debug()`
- Categories: `LOG_CATEGORY_KEY_POINT` (lifecycle events), `LOG_CATEGORY_VENDOR` (vendor status)
- Config log messages must use the `config:` prefix — QA automation matches on `(?:config|LOG_CATEGORY_KEY_POINT|KEYPOINT config):` to validate vendor config output:
  ```python
  ten_env.log_info(f"config: {self.config.to_str(sensitive_handling=True)}", category=LOG_CATEGORY_KEY_POINT)
  ```
- All output goes to `/tmp/task_run.log` inside the container

## Error Classification

Use `ModuleErrorCode` consistently:

| Situation | Severity | Notes |
| --------- | -------- | ----- |
| Missing or invalid required config | `FATAL_ERROR` | Extension should not continue |
| Auth failure (`401`, `403`, invalid API key) | `FATAL_ERROR` | Include `vendor_info` when available |
| Transient vendor disconnect / timeout | `NON_FATAL_ERROR` | Retry or reconnect if supported |
| Retry ceiling reached | `FATAL_ERROR` | Surface terminal failure clearly |
| Invalid user input for a single request | `NON_FATAL_ERROR` | Request fails, extension keeps running |

Provider-originated failures should include `vendor_info` when possible:

```python
ModuleErrorVendorInfo(vendor=self.vendor(), code="401", message="Unauthorized")
```

Before changing corner-case behavior such as empty TTS input, check the guarder
tests first. Repo test expectations override generic assumptions.

## Import Convention

```python
# Correct (v0.11+)
from ten_runtime import Addon, register_addon_as_extension, TenEnv

# Wrong (old v0.8.x — will not work)
from ten import Addon
```

## Formatting

- **Black** formatter with `--line-length 80`
- Run: `task format` (from `ai_agents/`)
- Check: `task check`
- Excludes: `third_party/`, `http_server_python/`, `ten_packages/system`

## Design Principles

- **YAGNI**: Only implement what is needed now, not what might be needed later
- **KISS**: Prefer simple solutions; three similar lines > premature abstraction
- **No git-ignored files**: Never modify auto-generated files (manifest-lock.json, out/, .ten/, bin/)

## Related Deep Dives

- [Extension Development](deep_dives/extension_development.md) — Full creation guide with implementation walkthroughs
