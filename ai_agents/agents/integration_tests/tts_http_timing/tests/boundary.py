"""Observe TEN outputs against timestamps supplied by the local mock provider."""

import asyncio
import json
import time
from typing_extensions import override
from ten_runtime import AsyncExtensionTester, Data, TenError, TenErrorCode


class ProviderReference:
    def __init__(self):
        self.request_id = None
        self.sent = {}
        self.audio = {}

    def begin_request(self, request_id):
        self.request_id = request_id

    def text_sent(self):
        self.sent.setdefault(self.request_id, time.monotonic_ns())

    def audio_received(self):
        self.audio.setdefault(self.request_id, time.monotonic_ns())

    def elapsed_ms(self, request_id):
        if request_id not in self.sent or request_id not in self.audio:
            return None
        return (self.audio[request_id] - self.sent[request_id]) / 1_000_000


class SequentialTTFBTester(AsyncExtensionTester):
    """Check reported TTFB against first audio received for two requests."""

    def __init__(self, reference=None):
        super().__init__()
        self.reference = reference
        self.request_ids = ("ttfb_streamed", "ttfb_single_final")
        self.sent_at: dict[str, float] = {}
        self.first_audio_at: dict[str, float] = {}
        self.reported_ttfb: dict[str, float] = {}
        self.audio_request_id: str | None = None
        self.finished: set[str] = set()
        self.watchdog: asyncio.Task | None = None

    async def _send(self, ten_env, request_id, text, final):
        data = Data.create("tts_text_input")
        data.set_property_from_json(
            "",
            json.dumps(
                {
                    "request_id": request_id,
                    "text": text,
                    "text_input_end": final,
                    "metadata": {
                        "session_id": "ttfb_guarder",
                        "turn_id": self.request_ids.index(request_id) + 1,
                    },
                }
            ),
        )
        if text and request_id not in self.sent_at:
            if self.reference is not None:
                self.reference.begin_request(request_id)
            self.sent_at[request_id] = time.monotonic()
        await ten_env.send_data(data)

    @override
    async def on_start(self, ten_env):
        async def timeout():
            await asyncio.sleep(20)
            ten_env.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric,
                    "Timed out waiting for both requests' audio and TTFB metrics",
                )
            )

        self.watchdog = asyncio.create_task(timeout())
        await self._send(ten_env, self.request_ids[0], "Hello world.", False)
        await self._send(ten_env, self.request_ids[0], "", True)

    def _check(self, ten_env):
        for request_id in self.request_ids:
            if (
                request_id not in self.first_audio_at
                or request_id not in self.reported_ttfb
            ):
                continue
            elapsed_ms = (
                self.first_audio_at[request_id] - self.sent_at[request_id]
            ) * 1000
            ttfb = self.reported_ttfb[request_id]
            if not 0 <= ttfb <= elapsed_ms:
                ten_env.stop_test(
                    TenError.create(
                        TenErrorCode.ErrorCodeGeneric,
                        f"{request_id}: reported TTFB {ttfb}ms exceeds observed first-audio delay {elapsed_ms:.1f}ms",
                    )
                )
                return
            if self.reference is not None:
                expected = self.reference.elapsed_ms(request_id)
                if expected is None or abs(ttfb - expected) > 5:
                    print(
                        f"TTFB mismatch: request={request_id}, reported={ttfb}ms, "
                        f"reference={expected}ms, tolerance=5ms",
                        flush=True,
                    )
                    ten_env.stop_test(
                        TenError.create(
                            TenErrorCode.ErrorCodeGeneric,
                            f"{request_id}: reported TTFB {ttfb}ms differs from provider-boundary reference {expected}ms",
                        )
                    )
                    return
        if all(
            request_id in self.finished
            and request_id in self.first_audio_at
            and request_id in self.reported_ttfb
            for request_id in self.request_ids
        ):
            ten_env.stop_test()

    @override
    async def on_data(self, ten_env, data):
        payload_str, _ = data.get_property_to_json("")
        payload = json.loads(payload_str)
        match data.get_name():
            case "error":
                ten_env.stop_test(
                    TenError.create(TenErrorCode.ErrorCodeGeneric, "TTS returned error")
                )
            case "tts_audio_start":
                self.audio_request_id = payload["request_id"]
            case "metrics":
                if "ttfb" in payload.get("metrics", {}):
                    turn_id = payload.get("metadata", {}).get("turn_id")
                    if turn_id not in (1, 2):
                        ten_env.stop_test(
                            TenError.create(
                                TenErrorCode.ErrorCodeGeneric,
                                "TTFB has no matching turn_id",
                            )
                        )
                        return
                    request_id = self.request_ids[turn_id - 1]
                    if request_id in self.reported_ttfb:
                        ten_env.stop_test(
                            TenError.create(
                                TenErrorCode.ErrorCodeGeneric,
                                "Duplicate request TTFB",
                            )
                        )
                        return
                    self.reported_ttfb[request_id] = payload["metrics"]["ttfb"]
                self._check(ten_env)
            case "tts_audio_end":
                request_id = payload["request_id"]
                self.finished.add(request_id)
                if request_id == self.request_ids[0]:
                    # Separate the requests enough to expose reuse of the old start.
                    await asyncio.sleep(0.15)
                    await self._send(ten_env, self.request_ids[1], "Hello again.", True)
                self._check(ten_env)

    @override
    async def on_audio_frame(self, ten_env, frame):
        if frame.get_buf() and self.audio_request_id is not None:
            self.first_audio_at.setdefault(self.audio_request_id, time.monotonic())
            self._check(ten_env)

    @override
    async def on_stop(self, ten_env):
        if self.watchdog is not None:
            self.watchdog.cancel()
