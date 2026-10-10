# Subtitle timing checks

As of: 2026-10-10

Run with `--enable_subtitle_alignment=True`. The test observes two sequential
requests through public TEN messages and audio frames. Each request has its own
PCM origin, metadata, and terminal subtitle result.

The generic checks verify:

- Every audio timestamp follows the first frame plus cumulative PCM samples,
  within one millisecond. Comparing only adjacent rounded frame durations can
  miss cumulative drift.
- Subtitle words and positive result ranges stay within the actual PCM sample
  span. A word may start after leading silence; it need not equal the first
  audio timestamp. Empty terminal markers do not represent spoken text and are
  excluded from timing-range checks.
- Request identity, metadata, sequence order, and terminal results remain valid.

For `minimax_tts_websocket_duplex`, the registry creates a fresh timing hook
only within this test. It temporarily replaces the client module's
`websockets` dependency binding with a proxy. Matching endpoint connections
receive a dedicated connection type through the public `create_connection`
parameter; the original connect object retains its await, context-manager,
and reconnect semantics. The shared WebSocket library and its global
`ClientConnection.send/recv` methods are not modified.

The hook observes unchanged public messages, binds them to known TEN test
inputs, and keeps connection and request capture separate. It reads the
`task_start` audio format, PCM bytes, sentence boundaries, original word times,
and flush completion. It returns immutable request-relative reference data;
the common checker compares that reference with TEN output. Nonzero first-word
offsets must be retained. Capture errors fail the timing check without raising
new exceptions into the extension's wire path.

The dependency binding is restored when the runner exits, including failure
paths. Only MiniMax Duplex is registered. Other vendors do not import or
install its hook, and other Guarder cases do not inject any hook. Existing
enable/skip behavior and generic checks remain the same. Tests sharing one
extension module must run sequentially; parallel vendor runs require separate
processes and installed app directories.

| Observation point | Information read |
| --- | --- |
| TEN tester `on_data` | Public TTS results, audio start/end, request metadata |
| TEN tester `on_audio_frame` | Public timestamps, samples, format, metadata |
| Public WebSocket `send`/`recv` | Untouched provider format, PCM, word times, boundaries |
| TEN tester `run` result | Success or the externally returned rejection diagnostic |

No observation uses production extension private members or methods. The
Guarder naturally runs its own validators to judge received public messages.
The hook is test-process dependency injection at a transport boundary, rather
than a strict test using only TEN input/output ports.

These checks protect timestamp conversion and the PCM timeline. They do not
perform phonetic forced alignment or prove that a provider's word annotations
match every acoustic onset. Other supported providers receive the generic
checks; the raw protocol observer currently applies only to MiniMax Duplex.
