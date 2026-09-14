import asyncio
import json
import os
import unicodedata
from pathlib import Path

import pytest
from ten_packages.extension.rtzr_asr_python.client import RTZRClient
from ten_packages.extension.rtzr_asr_python.config import RTZRASRConfig

from .test_extension import RecognitionTester

pytestmark = [
    pytest.mark.live,
    pytest.mark.skipif(
        os.getenv("RTZR_RUN_LIVE") != "1",
        reason="explicit live opt-in required",
    ),
]


def live_config(model):
    return RTZRASRConfig(
        params={
            "client_id": os.environ["RTZR_CLIENT_ID"],
            "client_secret": os.environ["RTZR_CLIENT_SECRET"],
            "api_base": os.environ["RTZR_API_BASE"],
            "websocket_url": os.environ.get("RTZR_WEBSOCKET_URL", ""),
            "model_name": model,
        }
    )


@pytest.mark.parametrize("model", ["sommers_ko", "sommers_ja", "sommers_en"])
async def test_live_handshake(model):
    client = RTZRClient(live_config(model))
    try:
        ws = await client.connect()
        assert not ws.closed
        await ws.send_str("EOS")
        await ws.close()
    finally:
        await client.close()


@pytest.mark.parametrize(
    "model,language",
    [("sommers_ko", "ko-KR"), ("sommers_ja", "ja-JP"), ("sommers_en", "en-US")],
)
def test_live_recognition(model, language):
    manifest = os.getenv("RTZR_AUDIO_MANIFEST")
    if not manifest:
        pytest.skip("RTZR_AUDIO_MANIFEST is required")
    samples = json.loads(Path(manifest).read_text())[model]
    tester = RecognitionTester()
    tester.audio_samples = [Path(item["pcm"]).read_bytes() for item in samples]
    tester.cycles = len(samples)
    tester.set_test_mode_single(
        "rtzr_asr_python", live_config(model).model_dump_json()
    )
    error = tester.run()
    output = Path(manifest).parent / f"{model}_ten_results.json"
    output.write_text(
        json.dumps(
            {
                "results": tester.results,
                "ends": tester.ends,
                "errors": tester.errors,
                "metrics": tester.metrics,
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    assert error is None, error.error_message() if error else ""
    assert len(tester.ends) == len(samples)
    finals = [item for item in tester.results if item["final"]]
    assert len(finals) >= len(samples)
    assert all(
        item["language"] == language
        and item["metadata"]["session_id"] == "session"
        for item in finals
    )
    assert all(item["text"] and item["duration_ms"] > 0 for item in finals)
    assert [item["start_ms"] for item in finals] == sorted(
        item["start_ms"] for item in finals
    )


@pytest.mark.parametrize("model", ["sommers_ko", "sommers_ja", "sommers_en"])
def test_live_long_stream(model):
    if os.getenv("RTZR_RUN_LONG") != "1":
        pytest.skip("RTZR_RUN_LONG=1 is required")
    manifest = Path(os.environ["RTZR_AUDIO_MANIFEST"])
    sample = json.loads(manifest.read_text())[model][0]
    audio = Path(sample["pcm"]).read_bytes() + bytes(32000)
    tester = RecognitionTester()
    size = 300 * 32000
    tester.audio = (audio * (size // len(audio) + 1))[:size]
    tester.cycles = 1
    tester.set_timeout(6 * 60 * 1000 * 1000)
    tester.set_test_mode_single(
        "rtzr_asr_python", live_config(model).model_dump_json()
    )
    error = tester.run()
    (manifest.parent / f"{model}_long_results.json").write_text(
        json.dumps(
            {
                "requested_audio_seconds": 300,
                "sent_audio_seconds": tester.sent_audio_bytes / 32000,
                "results": tester.results,
                "ends": tester.ends,
                "errors": tester.errors,
                "metrics": tester.metrics,
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    assert error is None, error.error_message() if error else ""
    finals = [item for item in tester.results if item["final"]]
    assert finals and finals[-1]["start_ms"] > 270000
    assert len(tester.ends) == 1
    assert [item["current"] for item in tester.statuses].count("connected") == 1


def normalized(text):
    text = unicodedata.normalize("NFKC", text).casefold()
    return "".join(char for char in text if char.isalnum())


def character_error_rate(reference, hypothesis):
    reference, hypothesis = normalized(reference), normalized(hypothesis)
    previous = list(range(len(hypothesis) + 1))
    for index, left in enumerate(reference, 1):
        current = [index]
        for other, right in enumerate(hypothesis, 1):
            current.append(
                min(
                    current[-1] + 1,
                    previous[other] + 1,
                    previous[other - 1] + (left != right),
                )
            )
        previous = current
    return previous[-1] / max(1, len(reference))


@pytest.mark.parametrize("model", ["sommers_ko", "sommers_ja", "sommers_en"])
async def test_live_direct_comparison(model):
    manifest_path = os.getenv("RTZR_AUDIO_MANIFEST")
    if not manifest_path:
        pytest.skip("RTZR_AUDIO_MANIFEST is required")
    manifest = Path(manifest_path)
    samples = json.loads(manifest.read_text())[model]
    client = RTZRClient(live_config(model))
    results = []
    receiver = None
    try:
        ws = await client.connect()

        async def receive():
            async for message in ws:
                if isinstance(message.data, str):
                    results.append(json.loads(message.data))

        receiver = asyncio.create_task(receive())
        for sample in samples:
            audio = Path(sample["pcm"]).read_bytes()
            for offset in range(0, len(audio), 320):
                await ws.send_bytes(audio[offset : offset + 320])
                await asyncio.sleep(0.01)
            await ws.send_json({"type": "Finalize"})
            await asyncio.sleep(5)
        await ws.send_str("EOS")
        await asyncio.wait_for(receiver, 10)
        await ws.close()
    finally:
        if receiver:
            receiver.cancel()
            await asyncio.gather(receiver, return_exceptions=True)
        await client.close()
    direct = " ".join(
        item["alternatives"][0]["text"]
        for item in results
        if item.get("final") and item.get("alternatives")
    )
    ten_results = json.loads(
        (manifest.parent / f"{model}_ten_results.json").read_text()
    )["results"]
    ten = " ".join(item["text"] for item in ten_results if item["final"])
    reference = " ".join(item["reference"] for item in samples)
    report = {
        "model": model,
        "reference": reference,
        "direct_text": direct,
        "ten_text": ten,
        "direct_cer": character_error_rate(reference, direct),
        "ten_cer": character_error_rate(reference, ten),
        "normalized_equal": normalized(direct) == normalized(ten),
        "direct_results": results,
    }
    (manifest.parent / f"{model}_comparison.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2)
    )
    assert direct and ten
    assert report[
        "normalized_equal"
    ], "Direct/TEN transcript differs; inspect comparison artifact"
