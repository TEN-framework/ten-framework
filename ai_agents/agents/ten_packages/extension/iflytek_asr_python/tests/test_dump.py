import asyncio
import stat
from pathlib import Path
from unittest.mock import patch

from .helpers import create_config, create_extension, make_audio_frame


def test_dump_preserves_multiple_frames_byte_for_byte_and_in_order(
    tmp_path: Path,
) -> None:
    async def run() -> None:
        config = create_config(dump=True, dump_path=str(tmp_path))
        extension, _, client = create_extension(config=config)
        frames = [bytes([index]) * 320 for index in range(1, 6)]

        await extension._start_audio_dumper()
        for content in frames:
            assert await extension.send_audio(make_audio_frame(content), None)
        await extension._stop_audio_dumper()

        expected = b"".join(frames)
        dump_file = tmp_path / "iflytek_asr_in.pcm"
        assert dump_file.stat().st_size == len(expected)
        assert dump_file.read_bytes() == expected
        assert client.audio == frames

    asyncio.run(run())


def test_dump_uses_private_subdirectory_for_shared_temp_root(
    tmp_path: Path,
) -> None:
    async def run() -> None:
        config = create_config(dump=True, dump_path=str(tmp_path))
        extension, _, _ = create_extension(config=config)

        with patch(
            "iflytek_asr_python.extension.tempfile.gettempdir",
            return_value=str(tmp_path),
        ):
            await extension._start_audio_dumper()

        assert extension.audio_dumper is not None
        dump_file = Path(extension.audio_dumper.dump_file_path)
        assert dump_file.name == "iflytek_asr_in.pcm"
        assert dump_file.parent.parent == tmp_path
        assert dump_file.parent.name.startswith("iflytek_asr_")
        assert stat.S_IMODE(dump_file.parent.stat().st_mode) == 0o700

        await extension._stop_audio_dumper()

    asyncio.run(run())
