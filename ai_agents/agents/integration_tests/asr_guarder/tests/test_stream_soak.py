"""Opt-in real-time soak test: TEN_ASR_SOAK_SECONDS=3600."""

import asyncio
import json
import os
import time
from pathlib import Path

import pytest
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override


class StreamSoakTester(AsyncExtensionTester):
    def __init__(
        self,
        audio: bytes,
        seconds: int,
        finalize_seconds: int,
        session_id: str = "asr-stream-soak",
    ):
        super().__init__()
        self.audio = audio
        self.seconds = seconds
        self.finalize_frames = finalize_seconds * 100
        self.session_id = session_id
        self.ready = asyncio.Event()
        self.actual_send_complete = asyncio.Event()
        self.finalize_complete: dict[str, asyncio.Event] = {}
        self.finalize_tasks: set[asyncio.Task] = set()
        self.sent_bytes = 0
        self.results = 0
        self.finals = 0
        self.actual_send_ms = 0
        self.started: float | None = None
        self.last_result: float | None = None
        self.last_final: float | None = None
        self.errors: list[dict] = []
        self.connections: list[dict] = []
        self.completed = False
        self.failure: str | None = None

    def snapshot(self) -> dict:
        rss_kib = None
        with open("/proc/self/status", encoding="utf-8") as status:
            for line in status:
                if line.startswith("VmRSS:"):
                    rss_kib = int(line.split()[1])
                    break
        return {
            "completed": self.completed,
            "failure": self.failure,
            "target_seconds": self.seconds,
            "elapsed_seconds": (
                round(time.monotonic() - self.started, 3)
                if self.started is not None
                else 0
            ),
            "sent_audio_seconds": self.sent_bytes / 32000,
            "sent_bytes": self.sent_bytes,
            "actual_send_ms": self.actual_send_ms,
            "results": self.results,
            "finals": self.finals,
            "finalize_requests": len(self.finalize_complete),
            "finalize_acks": sum(
                event.is_set() for event in self.finalize_complete.values()
            ),
            "last_result_age_seconds": (
                round(time.monotonic() - self.last_result, 3)
                if self.last_result is not None
                else None
            ),
            "last_final_age_seconds": (
                round(time.monotonic() - self.last_final, 3)
                if self.last_final is not None
                else None
            ),
            "rss_kib": rss_kib,
            "errors": self.errors,
            "connections": self.connections,
        }

    def fail(self, env: AsyncTenEnvTester, message: str) -> None:
        if self.failure is None:
            self.failure = message
            env.stop_test(
                TenError.create(TenErrorCode.ErrorCodeGeneric, message)
            )

    async def finalize(self, env: AsyncTenEnvTester, finalize_id: str) -> None:
        event = asyncio.Event()
        self.finalize_complete[finalize_id] = event
        data = Data.create("asr_finalize")
        data.set_property_from_json(
            None,
            json.dumps(
                {
                    "finalize_id": finalize_id,
                    "metadata": {"session_id": self.session_id},
                }
            ),
        )
        try:
            await asyncio.wait_for(env.send_data(data), 15)
            await asyncio.wait_for(event.wait(), 15)
        except asyncio.TimeoutError:
            self.fail(env, f"Finalize did not complete: {finalize_id}")

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        try:
            await asyncio.wait_for(self.ready.wait(), 30)
            self.started = time.monotonic()
            print("SOAK_STARTED " + json.dumps(self.snapshot()), flush=True)
            position = 0
            for index in range(self.seconds * 100):
                deadline = self.started + index / 100
                await asyncio.sleep(max(0, deadline - time.monotonic()))
                if time.monotonic() - deadline > 5:
                    self.fail(
                        ten_env, "Audio sender fell over 5 seconds behind"
                    )
                    return
                end = position + 320
                chunk = self.audio[position:end]
                if end > len(self.audio):
                    chunk += self.audio[: end - len(self.audio)]
                position = end % len(self.audio)
                frame = AudioFrame.create("pcm_frame")
                frame.set_sample_rate(16000)
                frame.set_bytes_per_sample(2)
                frame.set_number_of_channels(1)
                frame.set_samples_per_channel(160)
                frame.set_timestamp(index * 10)
                frame.set_property_from_json(
                    "metadata", json.dumps({"session_id": self.session_id})
                )
                frame.alloc_buf(320)
                buffer = frame.lock_buf()
                buffer[:] = chunk
                frame.unlock_buf(buffer)
                await asyncio.wait_for(ten_env.send_audio_frame(frame), 5)
                self.sent_bytes += 320
                if (index + 1) % self.finalize_frames == 0:
                    task = asyncio.create_task(
                        self.finalize(ten_env, f"soak-{index + 1}")
                    )
                    self.finalize_tasks.add(task)
                    task.add_done_callback(self.finalize_tasks.discard)
                if (index + 1) % 6000 == 0:
                    print(
                        "SOAK_PROGRESS " + json.dumps(self.snapshot()),
                        flush=True,
                    )
                    if (
                        self.last_final is None
                        or time.monotonic() - self.last_final > 60
                    ):
                        self.fail(ten_env, "No recent final ASR result")
                        return
            if self.finalize_tasks:
                await asyncio.gather(*tuple(self.finalize_tasks))
            await self.finalize(ten_env, "soak-end")
            await asyncio.wait_for(self.actual_send_complete.wait(), 15)
            if self.errors:
                self.fail(ten_env, "ASR reported errors during soak")
                return
            if self.failure is not None:
                return
            self.completed = True
            ten_env.stop_test()
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            self.fail(ten_env, f"Soak sender failed: {type(exc).__name__}")

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        raw, _ = data.get_property_to_json(None)
        payload = json.loads(raw)
        if name == "asr_ready":
            self.ready.set()
        elif name == "connection_status_changed":
            if payload.get("current") == "connected":
                self.ready.set()
            self.connections.append(
                {
                    "elapsed_seconds": (
                        round(time.monotonic() - self.started, 3)
                        if self.started is not None
                        else 0
                    ),
                    "last": payload.get("last"),
                    "current": payload.get("current"),
                    "code": payload.get("code"),
                }
            )
        elif name == "error":
            # Report only codes, not credentials or vendor payloads.
            self.errors.append(
                {
                    "code": payload.get("code"),
                    "vendor_code": payload.get("vendor_info", {}).get("code"),
                }
            )
            if payload.get("code") == -1000:
                self.fail(ten_env, "ASR entered a fatal error state")
        elif name == "metrics":
            actual = payload.get("metrics", {}).get("actual_send")
            if actual is not None:
                self.actual_send_ms = actual
                if actual >= self.seconds * 1000:
                    self.actual_send_complete.set()
        elif name == "asr_finalize_end":
            finalize_id = payload.get("finalize_id")
            event = self.finalize_complete.get(finalize_id)
            if event is None or event.is_set():
                self.fail(
                    ten_env, "Unknown or duplicate finalize acknowledgement"
                )
                return
            if payload.get("metadata", {}).get("session_id") != self.session_id:
                self.fail(ten_env, "Finalize session mismatch")
                return
            event.set()
        elif name == "asr_result":
            required = {
                "id",
                "text",
                "final",
                "start_ms",
                "duration_ms",
                "language",
            }
            if not required.issubset(payload):
                self.fail(ten_env, "ASR result is missing required fields")
                return
            if payload.get("metadata", {}).get("session_id") != self.session_id:
                self.fail(ten_env, "ASR result session mismatch")
                return
            if payload["language"] != "en-US":
                self.fail(ten_env, "ASR result language mismatch")
                return
            self.results += 1
            self.last_result = time.monotonic()
            if payload["final"] and payload["text"]:
                self.finals += 1
                self.last_final = self.last_result

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        for task in tuple(self.finalize_tasks):
            task.cancel()
        if self.finalize_tasks:
            await asyncio.gather(
                *tuple(self.finalize_tasks), return_exceptions=True
            )
        report = json.dumps(self.snapshot(), indent=2)
        report_path = os.environ.get("TEN_ASR_SOAK_REPORT")
        if report_path:
            Path(report_path).write_text(report + "\n", encoding="utf-8")
        print("SOAK_RESULT " + json.dumps(self.snapshot()), flush=True)


def test_stream_soak(extension_name: str, config_dir: str) -> None:
    seconds = int(os.environ.get("TEN_ASR_SOAK_SECONDS", "0"))
    if seconds <= 0:
        pytest.skip("Set TEN_ASR_SOAK_SECONDS to enable the real-time soak")
    finalize_seconds = int(
        os.environ.get("TEN_ASR_SOAK_FINALIZE_SECONDS", "15")
    )
    assert finalize_seconds > 0
    audio = (Path(__file__).parent / "test_data/16k_en_us.pcm").read_bytes()
    assert len(audio) >= 320 and len(audio) % 2 == 0
    config = json.loads(
        (Path(config_dir) / "property_en.json").read_text(encoding="utf-8")
    )
    tester = StreamSoakTester(
        audio, seconds, finalize_seconds, f"{extension_name}-stream-soak"
    )
    tester.set_test_mode_single(extension_name, json.dumps(config))
    tester.set_timeout((seconds + 90) * 1000 * 1000)
    error = tester.run()
    assert error is None, error.error_message() if error else ""
    assert tester.completed, tester.snapshot()
    assert tester.sent_bytes == seconds * 32000
    assert tester.actual_send_ms >= seconds * 1000
    assert tester.finals > 0
    assert all(event.is_set() for event in tester.finalize_complete.values())
