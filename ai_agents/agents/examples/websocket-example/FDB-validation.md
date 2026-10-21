# FDB graph validation — 2026-10-21

Tested TEN source commit: `b81ffba8a`.
Packaged graph regression test: `b81ffba8a` (`tests/test_fdb_runtime.py`).
Agora Mind source commit: `2658e3ba1f68329be632745c91a3e635cad30bfc`.
Graph: `fdb_v3` in `tenapp/property.json`.
Canonical graph SHA-256 (sorted JSON, compact separators):
`b4d40893e7aa745fa2d3175d5a13d1c91b0b6ea4666ca292cd5beecb09c39b2d`.

## Results

| Check | Result |
| --- | --- |
| Graph, transport and bridge lifecycle tests | 46 passed, 9 opt-in skips |
| Existing bridge unit tests | 12 passed |
| Native TEN WebSocket command/event/PCM tests | 2 passed |
| Deepgram query and metadata regression tests | 6 passed |
| Existing ElevenLabs TTS extension suite | 9 passed, existing deprecation/mock warnings |
| API server `go test ./...` | Both packages compile; no test files |
| TEN graph/property and three manifest schema checks | Passed |
| Black on affected Python directories | Passed |
| Pylint on WebSocket and bridge extensions | 10.00/10 |
| Packaged graph regression tests | 8 passed in 61.75 seconds |
| Real Agora Mind backend graph test | 1 passed in 3.83 seconds |
| Real backend Taskfile entry (`test-fdb-real`) | 1 passed in 3.83 seconds |
| Runtime image graph, manifest and bridge/transport source hashes | Match the current checkout |

The native runtime smoke starts the API and the actual five graph nodes in the
runtime image. Local WebSocket fixtures replace the three external services;
the real ASR extension, Rust Agora Mind addon, bridge and TTS extension run.
It verifies `/graphs`, `/start`, Workspace/Session creation, PCM input, repeated
ASR finals and completed turns, exactly one `session.user_input` and TTS request
per distinct audio interval, and 6400 bytes of expected output PCM per response
at 16 kHz/mono/16-bit. It checks
the successful `tts_audio_end` reason and exact 200 ms duration, Session deletion,
and `/stop`. Two cases reuse the same audio port and Agora Mind Session ID with
fresh workers; their case metadata remains isolated and `/list` confirms cleanup.
One- and two-turn cases use identical speech in separate intervals. Finals are
also replayed after the last answer, with TEN generating new result IDs; these
replays produce no extra input or audio. The Agent's final text is verified in
the raw Agora Mind events, independently of TTS transcript support.

Four additional tests inject a rejected input, a submission timeout, a
disconnected Agora Mind socket, and an accepted input with no completed turn.
Each test checks the failure event or runner deadline, suppresses duplicate
submissions and TTS, stops the failed worker, and then completes a normal case
using the same audio port. Quick regressions also verify that late Session
creation results cannot restore state after disconnect/stop, and stale input
failures do not affect a replacement Session.

Two long-case tests configure a 3-second API idle timeout. After a first audio
turn they pause for 8 seconds, exceeding the cleanup scan and shutdown grace,
then complete a second turn on the same worker. One uses periodic HTTP `/ping`;
the other requests `timeout: -1` without pings. Both explicitly delete the Session
and stop the worker. Before the API fix, `timeout: -1` incorrectly fell back to
3 seconds and this regression reproduced the worker's automatic shutdown.

PCM boundary tests reject empty buffers and incomplete sample frames before TEN
allocation, covering mono/stereo and 16-/32-bit frame widths. The native TEN
WebSocket test sends three malformed chunks, receives their transport errors,
and then verifies a complete valid PCM roundtrip on the same connection.
Base64 decoding uses strict validation; illegal characters and non-string audio
fields are rejected before the audio callback. The same connection remains
usable after these errors.
Non-object input metadata is also rejected before frame allocation and callback
dispatch, with the same connection recovery guarantee.

The real backend test starts the compiled Agora Mind server and a local scripted
custom-model endpoint. It verifies that `benchmark.fdb_v3` is installed in the
`fdb` workspace profile, that `benchmark.fdb_v3` is rejected as a profile name,
that a missing prompt path fails Session creation, and that the valid prompt
executes `book_flight` before the final text reaches TEN TTS. It also checks the
tool request contains the previous tool result and the case ID is present in
the emitted FDB tool record.

The production Agora Mind checkout currently does not contain
`resources/prompts/fdb_v3.md`; the opt-in test creates a temporary prompt file
to exercise the server's relative prompt resolution. The benchmark run must
provide the real prompt under `AGORA_MIND_RESOURCES_ROOT` before using official
cases. The same test uses the checked-in `examples/fdb_workspace_profiles.json`
and real `capabilities/fdb` implementation from the Agora Mind checkout.

The regression is now checked into the repository. Run it with the local image:

```sh
task test-fdb-runtime
```

The observed Agora Mind operations for a successful one-turn case, in order, were:

```text
workspace.create
session.create
session.user_input
session.delete
```

A two-turn case has two `session.user_input` operations before deletion. Quick
tests also accept distinct intervals while the previous turn is still pending
and preserve repeated turns when the ASR interval is unknown.

The real backend probe intentionally sends `benchmark.fdb_v3` once to verify
that a capability name is rejected as a profile; that failed probe accounts for
the extra `workspace.create` visible in its raw operation log.

The integration plan specifies the tool log target
`agora_mind::fdb::tool_call`. The current Agora Mind source emits the verified
record at `agora_mind::fdb` instead. TEN does not rewrite or filter that server
log; the Agora Mind runner must either update the upstream target or explicitly
support the current target before strict tool-call collection is enabled.

The existing voice-assistant graph matches the configuration before this work.
Official audio, external provider quality, fake tool execution and benchmark
scores require the Agora Mind runner and real provider credentials; they are
outside this synthetic test.

## Image and package versions

Build image:
`ghcr.io/ten-framework/ten_agent_build:0.7.14`

Build-image digest:
`sha256:ae28ec069644d5acda9a4182426662a368b993aca39413db77d0bf12d60f941a`

Local runtime image: `agora-mind/fdb-ten-agent:ten-0.7.14`

Local image ID:
`sha256:e43e9d2a407864febf1ff9dc4d8a17ff54094466b895ac3c5f4a136829091411`

Published runtime image:
`hub.agoralab.co/agora-mind/fdb-ten-agent@sha256:938d16c40e4d0fbe74fae8555fe63e9dd7303e35ece78a69d79e095b356995bc`

Compilation and release completed using the common Dockerfile. Docker Hub's
registry endpoint timed out, so the image was published to the configured
`hub.agoralab.co` registry. The final bridge Python files, bridge manifest and
graph configuration were copied into the image after runtime tests verified the
registration, TTS metadata, lifecycle, failure reporting and ASR replay fixes.
Their hashes match the current checkout.
The final WebSocket extension also includes verified PCM frame validation and
strict base64 decoding.
The API server was rebuilt from the tested source using Go 1.22.3 and copied
into the image. Its SHA-256 is
`743e4083bb9a9aacb78ed1acbb92946cb005d26f53377686ab76452650837408`.

| Package | Version |
| --- | --- |
| `ten_runtime`, `ten_runtime_python` | `0.11.74` |
| `ten_ai_base` | `0.7.62` |
| `agora_mind_extension_rust` | `0.1.0` |
| `agora_mind_bridge_python` | `0.1.0` |
| `websocket_server` | `0.0.2` |
| `deepgram_asr_python` | `0.5.4` |
| `elevenlabs_tts2_python` | `0.4.3` |

ASR and TTS graph settings are Nova-3 multilingual and
`eleven_multilingual_v2` / `pcm_16000`. The synthetic smoke uses deterministic
service replies and measures routing and lifecycle behavior.

The 2026-10-21 host verification reran the graph, transport and bridge suite
(`46 passed, 9 skipped`), the bridge extension suite (`12 passed`), the native
WebSocket extension suite (`2 passed`), and the ElevenLabs extension suite
(`9 passed`, with the existing deprecation/resource warnings). The packaged
runtime regression passed all 8 cases against the rebuilt image. Go server
`go test ./...` passed in the TEN build container (`app` and `app/internal`, no
test files). Pulling the published reference returned the same image digest.

The image build used the standard command with
`USE_AGENT=agents/examples/websocket-example`; the build container required
`GOPROXY=https://goproxy.cn,direct` because `proxy.golang.org` timed out.
