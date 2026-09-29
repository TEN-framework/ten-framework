import asyncio
import json
import sys
import struct
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from ten_ai_base.helper import PCMWriter
from ten_ai_base.tts2 import RequestState
from ten_runtime import Data

from typecast_tts_python.config import TypecastTTSConfig
from typecast_tts_python.extension import TypecastTTSExtension
from typecast_tts_python.typecast_tts import TypecastTTSClient


@pytest.mark.parametrize("failure_phase", ["dump", "usage"])
def test_terminal_failure_retries_without_duplicate_end(
    failure_phase, tmp_path
):
    async def run():
        env = MagicMock()
        events = []
        complete = asyncio.Event()
        failed = False
        extension = TypecastTTSExtension("test")
        extension.ten_env = env
        extension.config = TypecastTTSConfig(
            dump=True,
            dump_path=str(tmp_path),
            params={"api_key": "key", "voice_id": "voice"},
        )
        extension.config.update_params()
        extension.config.validate()
        extension.client = TypecastTTSClient(extension.config, env)
        original_flush = PCMWriter.flush
        original_usage = extension.send_usage_metrics
        original_finish = extension.finish_request

        async def flush(writer):
            nonlocal failed
            if failure_phase == "dump" and not failed:
                failed = True
                raise OSError("temporary dump I/O failure")
            await original_flush(writer)

        async def usage(*args, **kwargs):
            nonlocal failed
            if failure_phase == "usage" and not failed:
                failed = True
                raise OSError("temporary terminal metrics failure")
            await original_usage(*args, **kwargs)

        async def finish(*args, **kwargs):
            await original_finish(*args, **kwargs)
            complete.set()

        async def send_data(data):
            body, _ = data.get_property_to_json()
            events.append((data.get_name(), json.loads(body)))

        env.send_data = AsyncMock(side_effect=send_data)
        env.send_audio_frame = AsyncMock()
        extension.send_usage_metrics = usage
        extension.finish_request = finish
        sdk = MagicMock()
        sdk.__aenter__ = AsyncMock(return_value=sdk)
        sdk.__aexit__ = AsyncMock()

        async def stream(request, chunk_size):
            assert chunk_size == extension.config.chunk_size
            assert request.text in ("first", "recovery")
            header = (
                b"RIFF"
                + b"\xff" * 4
                + b"WAVEfmt "
                + struct.pack("<IHHIIHH", 16, 1, 1, 32000, 64000, 2, 16)
                + b"data"
                + b"\xff" * 4
            )
            yield header + b"\x01\x02" * 3200

        sdk.text_to_speech_stream = stream
        with patch(
            "typecast_tts_python.typecast_tts.AsyncTypecast", return_value=sdk
        ), patch.object(PCMWriter, "flush", flush):
            loop = asyncio.create_task(extension._process_input_queue(env))
            try:
                for request_id in ("first", "recovery"):
                    complete.clear()
                    data = Data.create("tts_text_input")
                    data.set_property_from_json(
                        None,
                        json.dumps(
                            {
                                "request_id": request_id,
                                "text": request_id,
                                "text_input_end": True,
                            }
                        ),
                    )
                    await extension.on_data(env, data)
                    await asyncio.wait_for(complete.wait(), 1)
                    ends = [
                        body
                        for name, body in events
                        if name == "tts_audio_end"
                        and body["request_id"] == request_id
                    ]
                    assert len(ends) == 1
                    assert ends[0]["request_total_audio_duration_ms"] == 100
                    assert extension.request_states[request_id] == (
                        RequestState.COMPLETED
                    )
                    assert extension._processing_request_id is None
                    assert request_id not in extension.recorder_map
                    assert (
                        tmp_path / f"typecast_dump_{request_id}.pcm"
                    ).read_bytes() == b"\x01\x02" * 3200
                assert failed
                assert env.send_audio_frame.await_count == 2
            finally:
                await extension.input_queue.put(None)
                await asyncio.wait_for(loop, 1)
                await extension.client.clean()

    asyncio.run(run())
