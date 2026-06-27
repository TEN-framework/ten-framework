"""
Unit tests for BlazeRealtimeClient
"""

import json

import pytest

from blaze_stt_realtime_python.blaze_stt_realtime import (
    BlazeRealtimeClient,
    BlazeRealtimeClientConfig,
)


class TestBlazeRealtimeClient:
    """Test suite for the BlazeRealtimeClient websocket transport"""

    def test_init_with_config_dict(self, mock_config):
        """Test initialization with dict config"""
        stt = BlazeRealtimeClient(config=mock_config)
        assert stt.config.api_url == "http://localhost:8000"
        assert stt.config.api_key == "test-token"
        assert stt.config.default_language == "vi"
        assert stt.config.default_model == "stt-stream-1.5"
        assert stt.ws_url == "ws://localhost:8000/v1/stt/realtime"

    def test_init_with_config_object(self):
        """Test initialization with BlazeRealtimeClientConfig object"""
        config = BlazeRealtimeClientConfig(
            api_url="https://test.com",
            api_key="test-key",
            default_language="en",
            default_model="stt-stream-1.5",
        )
        stt = BlazeRealtimeClient(config=config)
        assert stt.config.api_url == "https://test.com"
        assert stt.config.api_key == "test-key"
        assert stt.config.default_language == "en"
        assert stt.ws_url == "wss://test.com/v1/stt/realtime"

    def test_init_with_env_vars(self, monkeypatch):
        """Test initialization with environment variables"""
        monkeypatch.setenv("BLAZE_STT_API_URL", "http://env-test.com")
        monkeypatch.setenv("BLAZE_STT_API_KEY", "env-key")

        stt = BlazeRealtimeClient(config=None)
        assert stt.config.api_url == "http://env-test.com"
        assert stt.config.api_key == "env-key"
        assert stt.ws_url == "ws://env-test.com/v1/stt/realtime"

    def test_build_ws_url(self):
        """Test http/https are converted to ws/wss"""
        build = BlazeRealtimeClient.build_ws_url
        assert build("http://h:8000") == "ws://h:8000/v1/stt/realtime"
        assert build("https://h") == "wss://h/v1/stt/realtime"
        assert build("ws://h") == "ws://h/v1/stt/realtime"

    async def test_transcribe_stream_yields_events(
        self, patch_ws, mock_config, ready_partial_final
    ):
        """transcribe_stream() sends init + audio and yields all events"""
        ws = patch_ws(ready_partial_final)
        stt = BlazeRealtimeClient(config=mock_config)

        events = []
        async for event in stt.transcribe_stream(
            [b"chunk-1", b"chunk-2"], drain_timeout=0.1
        ):
            events.append(event)

        # Yielded events match the queued server messages
        types = [e["type"] for e in events]
        assert types == ["ready", "partial", "final"]
        assert events[-1]["text"] == "Xin chào, tôi là trợ lý ảo."

        # Init message was sent first as JSON with the expected fields
        init = json.loads(ws.sent_text[0])
        assert init["token"] == "test-token"
        assert init["language"] == "vi"
        assert init["model"] == "stt-stream-1.5"
        assert init["enable_log"] is False

        # Audio chunks were forwarded as binary
        assert ws.sent_audio == [b"chunk-1", b"chunk-2"]

    async def test_transcribe_stream_overrides(
        self, patch_ws, mock_config, ready_partial_final
    ):
        """language/model/enable_log overrides land in the init message"""
        ws = patch_ws(ready_partial_final)
        stt = BlazeRealtimeClient(config=mock_config)

        async for _ in stt.transcribe_stream(
            [b"a"],
            language="en",
            model="custom-model",
            enable_log=True,
            drain_timeout=0.1,
        ):
            pass

        init = json.loads(ws.sent_text[0])
        assert init["language"] == "en"
        assert init["model"] == "custom-model"
        assert init["enable_log"] is True

    async def test_transcribe_stream_error_before_ready(
        self, patch_ws, mock_config
    ):
        """An error before 'ready' raises ValueError"""
        patch_ws(
            [json.dumps({"type": "error", "text": "Authentication failed"})]
        )
        stt = BlazeRealtimeClient(config=mock_config)

        with pytest.raises(ValueError, match="Authentication failed"):
            async for _ in stt.transcribe_stream([b"a"], drain_timeout=0.1):
                pass

    async def test_transcribe_accumulates_final(
        self, patch_ws, mock_config, sample_pcm
    ):
        """transcribe() chunks the buffer and accumulates final transcripts"""
        ws = patch_ws(
            [
                json.dumps({"type": "ready", "text": "ok"}),
                json.dumps({"type": "partial", "text": "xin"}),
                json.dumps({"type": "final", "text": "Xin chào"}),
                json.dumps({"type": "final", "text": "thế giới"}),
            ]
        )
        stt = BlazeRealtimeClient(config=mock_config)

        result = await stt.transcribe(
            audio_data=sample_pcm,
            chunk_size=3200,
            chunk_interval=0,
            drain_timeout=0.1,
        )

        assert result["transcription"] == "Xin chào thế giới"
        assert result["finals"] == ["Xin chào", "thế giới"]
        assert result["partials"] == ["xin"]
        # 8000 bytes / 3200 -> 3 chunks
        assert len(ws.sent_audio) == 3

    async def test_transcribe_empty_raises(self, mock_config):
        """transcribe() with empty audio raises ValueError"""
        stt = BlazeRealtimeClient(config=mock_config)
        with pytest.raises(ValueError, match="audio_data cannot be empty"):
            await stt.transcribe(audio_data=b"")

    async def test_process_method(self, patch_ws, mock_config, sample_pcm):
        """process() (TEN interface) returns the accumulated transcription"""
        patch_ws(
            [
                json.dumps({"type": "ready", "text": "ok"}),
                json.dumps({"type": "final", "text": "Xin chào"}),
            ]
        )
        stt = BlazeRealtimeClient(config=mock_config)

        result = await stt.process(
            {
                "audio_data": sample_pcm,
                "language": "vi",
            }
        )

        assert result["transcription"] == "Xin chào"
        assert result["status"] == "completed"
        assert "raw_result" in result

    async def test_process_missing_audio_data(self, mock_config):
        """process() raises when audio_data is missing"""
        stt = BlazeRealtimeClient(config=mock_config)
        with pytest.raises(
            ValueError, match="audio_data is required in input_data"
        ):
            await stt.process({})

    def test_get_metadata(self, mock_config):
        """get_metadata() reports realtime capabilities"""
        stt = BlazeRealtimeClient(config=mock_config)
        metadata = stt.get_metadata()

        assert metadata["name"] == "blaze_stt_realtime_python"
        assert metadata["version"] == "1.0.0"
        assert "realtime" in metadata["capabilities"]
        assert "streaming" in metadata["capabilities"]
        assert metadata["transport"] == "websocket"
        assert metadata["endpoint"] == "/v1/stt/realtime"
        assert metadata["audio_format"]["sample_rate"] == 16000
        assert "vi" in metadata["supported_languages"]
