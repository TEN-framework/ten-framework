# TTFB guarder references

As of: 2026-11-03 (Asia/Shanghai, UTC+08:00)
Codebase commit: `10c89b121`

HTTP adapters use a simple interval: calling the public client `get()` with
nonempty text through its first nonempty audio `RESPONSE`. This includes
request preparation, connection waiting, DNS/TCP/TLS, and first-audio parsing
and conversion. The base records the interval; adapters need no timing hooks.

The guarder wraps this public adapter input/output boundary, independently
records monotonic times, and does not read production timing state. It is not
a wire-level latency measurement. It requires agreement within 1 ms and the
strict tester send-to-first-audio outer bound for two sequential requests.
Empty responses and non-audio events do not finish the interval.

HTTP coverage: OpenAI, Mistral, EZAI, Hume, Generic HTTP and Gemini HTTP.
Google and Cosy retain their public SDK observers. Azure requires a prepared
native mock SDK library; no independent live Azure reference is claimed.

Run in the container from `ai_agents/` with credentials loaded:

```bash
task tts-guarder-test EXTENSION=openai_tts2_python -- \
  -k test_sequential_requests_ttfb -m api --strict-markers -v --tb=short
```

Replace EXTENSION with the target. Use a prepared local base until the updated
package is published. Do not commit credentials or installed package copies.
