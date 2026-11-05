# LLM extension

## Templates

| Vendor API | Copy from | Why |
| ---------- | --------- | --- |
| OpenAI-compatible chat completions | `openai_llm2_python` | Used by most example graphs; `base_url` swap covers many vendors |
| Native SDK with streaming + tools | `anthropic_llm2_python` | Startup race handling, runtime-driven tests |

If the vendor is OpenAI-compatible, first check whether configuring
`openai_llm2_python` (`base_url`, `api_key`, `model`) is enough. Only create a
new package when the wire format or features differ.

## Package layout

```text
<vendor>_llm2_python/
├── __init__.py
├── addon.py
├── extension.py         # <Vendor>LLM2Extension(AsyncLLM2BaseExtension)
├── <vendor>_llm.py      # <Vendor>LLM2Config + <Vendor>LLM client (request build, stream parse)
├── manifest.json        # imports llm-interface.json
├── property.json        # flat keys: api_key, base_url, model, prompt, max_tokens, proxy_url, ...
├── pyproject.toml
├── requirements.txt
├── README.md
└── tests/
    ├── __init__.py
    ├── bin/start
    ├── conftest.py
    └── test_chat_completion.py
```

LLM extensions keep config **flat** (no `params` object) and the config model
lives in the client module, matching both templates. A separate `config.py` is
acceptable when the model grows beyond ~40 lines.

## extension.py lifecycle (fixed flow)

`AsyncLLM2BaseExtension` handles the `chat_completion` / `abort` /
`retrieve_prompt` commands. You implement:

```python
async def on_retrieve_prompt(self, ten_env, request: LLMRequestRetrievePrompt
                             ) -> LLMResponseRetrievePrompt: ...
def on_call_chat_completion(self, ten_env, request_input: LLMRequest
                            ) -> AsyncGenerator[LLMResponse, None]: ...
```

1. `on_start`: `await super().on_start(...)`; parse config; if invalid or the
   key is missing, log an error and return (never raise — an exception here
   exits the whole worker); build the client; **always** set a `_started`
   event in `finally`.
2. `on_call_chat_completion` returns an async generator that waits (bounded)
   for `_started`, raises a clear `RuntimeError` if the client is missing, then
   yields from `client.get_chat_completions(request_input)`.
3. `on_stop`: `await super().on_stop(...)` first (cancels in-flight streams),
   then close the client.

## Client (`<vendor>_llm.py`) responsibilities

- Convert `LLMRequest.messages` (text, images, `LLMMessageFunctionCall`,
  `LLMMessageFunctionCallOutput`) to the vendor format.
- Convert `LLMRequest.tools` (`LLMToolMetadata`) to the vendor tool schema.
- Stream vendor events into, in order:
  `LLMResponseMessageDelta` (text chunks), optional
  `LLMResponseReasoningDelta` / `LLMResponseReasoningDone`,
  `LLMResponseToolCall` (complete arguments), and exactly one
  `LLMResponseMessageDone` at the end. Carry `response_id` on every item.
- Honour `request_input.streaming` (stream vs single completion).
- Map 401/403 to a clear error message; never log the key or headers.

## Tests

Follow `anthropic_llm2_python/tests/test_chat_completion.py`: drive the addon
through `AsyncExtensionTester`, send a `chat_completion` `Cmd`, and parse
streamed `CmdResult`s with `parse_llm_response`.

- One test that runs **without credentials** and proves the addon loads,
  registers, and dispatches (error result when the key is missing).
- Real-API tests guarded with
  `pytest.mark.skipif(not API_KEY, reason="<VENDOR>_API_KEY is not set")`:
  streaming text, message-done ordering, tool call round trip,
  `retrieve_prompt`.

There is no LLM guarder. Run `task test-extension
EXTENSION=agents/ten_packages/extension/<name>` with the key exported.
