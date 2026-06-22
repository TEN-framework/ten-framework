import importlib.util
from pathlib import Path

import numpy as np


audio_mixer_spec = importlib.util.spec_from_file_location(
    "conversation_recorder_audio_mixer",
    Path(__file__).parent.parent / "audio_mixer.py",
)
audio_mixer_module = importlib.util.module_from_spec(audio_mixer_spec)
audio_mixer_spec.loader.exec_module(audio_mixer_module)
AudioMixer = audio_mixer_module.AudioMixer


def pcm(samples):
    return np.array(samples, dtype=np.int16).tobytes()


def samples(audio):
    return np.frombuffer(audio, dtype=np.int16)


def test_mixer_prebuffers_source_before_consuming_partial_audio():
    mixer = AudioMixer(
        sample_rate=1000,
        chunk_duration_ms=40,
        source_prebuffer_ms=120,
    )

    mixer.push_audio("assistant", pcm([100] * 60), 1000)

    assert mixer.mix_next_chunk() == b""
    assert len(mixer.buffers["assistant"].samples) == 60

    mixer.push_audio("assistant", pcm([100] * 60), 1000)
    mixed = samples(mixer.mix_next_chunk())

    assert len(mixed) == 40
    assert np.all(mixed == 100)
    assert len(mixer.buffers["assistant"].samples) == 80


def test_mixer_waits_to_rebuffer_after_source_underrun():
    mixer = AudioMixer(
        sample_rate=1000,
        chunk_duration_ms=40,
        source_prebuffer_ms=120,
    )

    mixer.push_audio("assistant", pcm([100] * 120), 1000)

    assert len(samples(mixer.mix_next_chunk())) == 40
    assert len(samples(mixer.mix_next_chunk())) == 40
    assert len(samples(mixer.mix_next_chunk())) == 40

    mixer.push_audio("assistant", pcm([100] * 20), 1000)

    assert mixer.mix_next_chunk() == b""
    assert len(mixer.buffers["assistant"].samples) == 20

    mixer.push_audio("assistant", pcm([100] * 100), 1000)

    assert len(samples(mixer.mix_next_chunk())) == 40


def test_mixer_drain_consumes_partial_tail_with_trailing_silence():
    mixer = AudioMixer(
        sample_rate=1000,
        chunk_duration_ms=40,
        source_prebuffer_ms=120,
    )

    mixer.push_audio("assistant", pcm([100] * 20), 1000)

    assert mixer.mix_next_chunk() == b""

    mixed = samples(mixer.mix_next_chunk(drain=True))

    assert len(mixed) == 40
    assert np.all(mixed[:20] == 100)
    assert np.all(mixed[20:] == 0)
    assert mixer.mix_next_chunk(drain=True) == b""
