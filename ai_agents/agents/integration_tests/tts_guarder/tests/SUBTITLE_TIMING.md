# Subtitle timing checks

As of: 2026-10-09 13:35 UTC
Codebase commit: `2ff14de45dabb2e70206557dd8b21cd081d63a29`

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

For MiniMax Duplex, passive observers wrap the public WebSocket send/receive
boundary. They return every message unchanged. The checker reads the actual
`task_start` audio format, PCM bytes, sentence boundaries, and original word
times. It compares those inputs with TEN output rather than trusting only the
extension's converted timestamps. Nonzero first-word offsets must be retained.

`test_guarder_timing.py` runs seven standalone TEN cases through the same
`SubtitleAlignmentTester` entry point as the live test. A test-only registered
extension in `timing_fixture/` exchanges actual loopback WebSocket messages
with a controlled provider and emits public TEN `Data` and `AudioFrame`
messages. It supplies valid traces and intentionally invalid traces for erased
word offsets, cumulative frame drift, and words or result ranges outside PCM.
The separate-origins case uses a long fractional first request and a short
second request. Every case includes two requests and empty terminal markers.

The regression functions assert only the public TEN runner's success or returned
error. A negative case must return the expected diagnostic, so a missing addon,
timeout, or unrelated setup error cannot make the case pass. They do not call
the timing validators directly, inspect the Guarder's captured lists, or read
private extension fields. These cases verify the Guarder's behavior; the live
case separately verifies production MiniMax output.

| Observation point | Information read |
| --- | --- |
| TEN tester `on_data` | Public TTS results, audio start/end, request metadata |
| TEN tester `on_audio_frame` | Public timestamps, samples, format, metadata |
| Public WebSocket `send`/`recv` | Untouched provider format, PCM, word times, boundaries |
| TEN tester `run` result | Success or the externally returned rejection diagnostic |

No observation uses production extension private members or methods. The
Guarder naturally runs its own validators to judge received messages; the
regression tests observe only its runner result. Mock provider and fixture
extension state belongs to test doubles, not the production extension.

These checks protect timestamp conversion and the PCM timeline. They do not
perform phonetic forced alignment or prove that a provider's word annotations
match every acoustic onset. Other supported providers receive the generic
checks; the raw protocol observer currently applies only to MiniMax Duplex.
