from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path


STRUCT_PATH = Path(__file__).resolve().parents[1] / "realtime" / "struct.py"
SPEC = spec_from_file_location("fpt_realtime_struct", STRUCT_PATH)
assert SPEC is not None and SPEC.loader is not None
STRUCT_MODULE = module_from_spec(SPEC)
SPEC.loader.exec_module(STRUCT_MODULE)

TranscriptMessage = STRUCT_MODULE.TranscriptMessage
parse_server_message = STRUCT_MODULE.parse_server_message


def test_parse_server_message_maps_asr_to_input_transcript() -> None:
    message = parse_server_message(
        """
        {
          "type": "asr",
          "text": "No.",
          "asr_start": "2026-03-14 16:44:36.302055",
          "asr_end": "2026-03-14 16:44:41.243055",
          "asr_record_time": "4.000",
          "asr_waiting_time": "0.941",
          "asr_delay_time": "1.941",
          "audio_id": "1773481477",
          "audio_file": "25f22708-641b-496d-85e2-424bd5a9ccb4_1773481477.wav",
          "message_id": 2
        }
        """
    )

    assert isinstance(message, TranscriptMessage)
    assert message.direction == "input"
    assert message.text == "No."
    assert message.final is True
    assert message.raw["message_id"] == 2


def test_parse_server_message_maps_tts_to_output_transcript() -> None:
    message = parse_server_message(
        """
        {
          "type": "tts",
          "text": "could you please repeat it for i,",
          "tts_start": "2026-03-14 16:44:37.863614",
          "tts_end": "2026-03-14 16:44:37.863615",
          "tts_start_play": "2026-03-14 16:44:39.959732",
          "tts_waiting_time": "0.000",
          "tts_waiting_time_play": "0.000",
          "message_id": 2
        }
        """
    )

    assert isinstance(message, TranscriptMessage)
    assert message.direction == "output"
    assert message.text == "could you please repeat it for i,"
    assert message.final is True
    assert message.raw["message_id"] == 2


def test_parse_server_message_keeps_keyword_fallback() -> None:
    message = parse_server_message(
        """
        {
          "type": "response_final",
          "role": "assistant",
          "text": "Done"
        }
        """
    )

    assert isinstance(message, TranscriptMessage)
    assert message.direction == "output"
    assert message.text == "Done"
    assert message.final is True
