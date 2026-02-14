"""
Unit tests for BlazeSTTExtension
"""
import pytest
from unittest.mock import Mock, patch, MagicMock
import httpx

from blaze_stt_python import BlazeSTTExtension, BlazeSTTConfig


class TestBlazeSTTExtension:
    """Test suite for BlazeSTTExtension"""
    
    def test_init_with_config_dict(self, mock_config):
        """Test initialization with dict config"""
        stt = BlazeSTTExtension(config=mock_config)
        assert stt.config.api_url == "http://localhost:8000"
        assert stt.config.api_key == "test-api-key"
        assert stt.config.default_language == "vi"
        assert stt.endpoint == "http://localhost:8000/v1/stt/execute"
    
    def test_init_with_config_object(self):
        """Test initialization with BlazeSTTConfig object"""
        config = BlazeSTTConfig(
            api_url="http://test.com",
            api_key="test-key",
            default_language="en",
        )
        stt = BlazeSTTExtension(config=config)
        assert stt.config.api_url == "http://test.com"
        assert stt.config.api_key == "test-key"
        assert stt.config.default_language == "en"
    
    def test_init_with_env_vars(self, monkeypatch):
        """Test initialization with environment variables"""
        monkeypatch.setenv("BLAZE_STT_API_URL", "http://env-test.com")
        monkeypatch.setenv("BLAZE_STT_API_KEY", "env-key")
        
        stt = BlazeSTTExtension(config=None)
        assert stt.config.api_url == "http://env-test.com"
        assert stt.config.api_key == "env-key"
    
    @patch('httpx.Client')
    def test_transcribe_with_bytes(self, mock_client_class, sample_audio_bytes, mock_api_response_completed):
        """Test transcribe() with bytes (binary mode)"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_completed
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        # Initialize extension
        stt = BlazeSTTExtension(config={
            "api_url": "http://localhost:8000",
            "api_key": "test-key",
        })
        
        # Call transcribe
        result = stt.transcribe(
            audio_data=sample_audio_bytes,
            audio_content_type="audio/wav",
            language="vi",
        )
        
        # Verify request was made correctly
        mock_client.post.assert_called_once()
        call_args = mock_client.post.call_args
        
        # Check endpoint
        assert call_args[0][0] == "http://localhost:8000/v1/stt/execute"
        
        # Check content (binary data)
        assert call_args[1]["content"] == sample_audio_bytes
        
        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Content-Type"] == "audio/wav"
        assert headers["Authorization"] == "Bearer test-key"
        
        # Check params
        params = call_args[1]["params"]
        assert params["language"] == "vi"
        assert params["enable_segments"] == "false"
        assert params["enable_refinement"] == "false"
        assert params["lazy_process"] == "false"
        
        # Verify result
        assert result["transcription"] == "Xin chào, đây là test transcription"
        assert result["job_status"] == "completed"
    
    @patch('httpx.Client')
    def test_transcribe_with_upload_file(self, mock_client_class, mock_upload_file, mock_api_response_completed):
        """Test transcribe() with UploadFile (multipart mode)"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_completed
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        # Initialize extension
        stt = BlazeSTTExtension(config={
            "api_url": "http://localhost:8000",
            "api_key": "test-key",
        })
        
        # Call transcribe with UploadFile
        result = stt.transcribe(
            audio_file=mock_upload_file,
            language="vi",
        )
        
        # Verify request was made correctly
        mock_client.post.assert_called_once()
        call_args = mock_client.post.call_args
        
        # Check endpoint
        assert call_args[0][0] == "http://localhost:8000/v1/stt/execute"
        
        # Check files (multipart)
        assert "files" in call_args[1]
        files = call_args[1]["files"]
        assert "audio_file" in files
        assert files["audio_file"][0] == "test_audio.wav"
        
        # Check headers (no Content-Type for multipart)
        headers = call_args[1]["headers"]
        assert "Content-Type" not in headers
        assert headers["Authorization"] == "Bearer test-key"
        
        # Verify result
        assert result["transcription"] == "Xin chào, đây là test transcription"
        assert result["job_status"] == "completed"
    
    @patch('httpx.Client')
    def test_transcribe_lazy_process(self, mock_client_class, sample_audio_bytes, mock_api_response_processing):
        """Test transcribe() with lazy_process=True"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_processing
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        # Initialize extension
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        # Call transcribe with lazy_process=True
        result = stt.transcribe(
            audio_data=sample_audio_bytes,
            lazy_process=True,
        )
        
        # Verify lazy_process parameter
        call_args = mock_client.post.call_args
        params = call_args[1]["params"]
        assert params["lazy_process"] == "true"
        
        # Verify result
        assert result["job_id"] == "test-job-id-123"
        assert result["job_status"] == "processing"
    
    def test_transcribe_no_input(self):
        """Test transcribe() with no input raises ValueError"""
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        with pytest.raises(ValueError, match="Either audio_data or audio_file must be provided"):
            stt.transcribe()
    
    def test_transcribe_empty_bytes(self):
        """Test transcribe() with empty bytes raises ValueError"""
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        with pytest.raises(ValueError, match="audio_data cannot be empty"):
            stt.transcribe(audio_data=b"")
    
    @patch('httpx.Client')
    def test_get_job_status(self, mock_client_class, mock_api_response_job_status):
        """Test get_job_status()"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_job_status
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.get.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        # Initialize extension
        stt = BlazeSTTExtension(config={
            "api_url": "http://localhost:8000",
            "api_key": "test-key",
        })
        
        # Call get_job_status
        result = stt.get_job_status("test-job-id-123")
        
        # Verify request was made correctly
        mock_client.get.assert_called_once()
        call_args = mock_client.get.call_args
        
        # Check endpoint
        assert call_args[0][0] == "http://localhost:8000/v1/stt/test-job-id-123"
        
        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Authorization"] == "Bearer test-key"
        
        # Verify result
        assert result["job_id"] == "test-job-id-123"
        assert result["job_status"] == "completed"
        assert result["transcription"] == "Xin chào, đây là test transcription"
    
    def test_process_method(self, sample_audio_bytes, mock_api_response_completed):
        """Test process() method (TEN framework interface)"""
        with patch('httpx.Client') as mock_client_class:
            # Setup mock response
            mock_response = Mock()
            mock_response.json.return_value = mock_api_response_completed
            mock_response.raise_for_status = Mock()
            
            mock_client = Mock()
            mock_client.__enter__ = Mock(return_value=mock_client)
            mock_client.__exit__ = Mock(return_value=False)
            mock_client.post.return_value = mock_response
            mock_client_class.return_value = mock_client
            
            # Initialize extension
            stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
            
            # Call process
            result = stt.process({
                "audio_data": sample_audio_bytes,
                "audio_content_type": "audio/wav",
                "language": "vi",
            })
            
            # Verify result format
            assert result["transcription"] == "Xin chào, đây là test transcription"
            assert result["status"] == "completed"
            assert "job_id" in result
    
    def test_process_method_missing_audio_data(self):
        """Test process() method raises error when audio_data is missing"""
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        with pytest.raises(ValueError, match="audio_data is required in input_data"):
            stt.process({})
    
    def test_get_metadata(self):
        """Test get_metadata() method"""
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        metadata = stt.get_metadata()
        
        assert metadata["name"] == "blaze_stt_python"
        assert metadata["version"] == "1.0.0"
        assert "stt" in metadata["capabilities"]
        assert "transcription" in metadata["capabilities"]
        assert "speech_to_text" in metadata["capabilities"]
        assert "audio/wav" in metadata["supported_formats"]
        assert "vi" in metadata["supported_languages"]
    
    @patch('httpx.Client')
    def test_transcribe_with_enable_segments(self, mock_client_class, sample_audio_bytes, mock_api_response_completed):
        """Test transcribe() with enable_segments=True"""
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_completed
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        result = stt.transcribe(
            audio_data=sample_audio_bytes,
            enable_segments=True,
        )
        
        call_args = mock_client.post.call_args
        params = call_args[1]["params"]
        assert params["enable_segments"] == "true"
    
    @patch('httpx.Client')
    def test_transcribe_with_enable_refinement(self, mock_client_class, sample_audio_bytes, mock_api_response_completed):
        """Test transcribe() with enable_refinement=True"""
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_completed
        mock_response.raise_for_status = Mock()
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        result = stt.transcribe(
            audio_data=sample_audio_bytes,
            enable_refinement=True,
        )
        
        call_args = mock_client.post.call_args
        params = call_args[1]["params"]
        assert params["enable_refinement"] == "true"
    
    @patch('httpx.Client')
    def test_transcribe_http_error(self, mock_client_class, sample_audio_bytes):
        """Test transcribe() handles HTTP errors"""
        # Setup mock response with error
        mock_response = Mock()
        mock_response.status_code = 400
        mock_response.text = "Bad Request"
        mock_response.raise_for_status.side_effect = httpx.HTTPStatusError(
            "Bad Request",
            request=Mock(),
            response=mock_response
        )
        
        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client
        
        stt = BlazeSTTExtension(config={"api_url": "http://localhost:8000"})
        
        with pytest.raises(httpx.HTTPStatusError):
            stt.transcribe(audio_data=sample_audio_bytes)

