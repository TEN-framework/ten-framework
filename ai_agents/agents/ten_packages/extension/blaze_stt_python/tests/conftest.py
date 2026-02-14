"""
Pytest fixtures for Blaze STT Extension tests
"""
import io
import pytest
from typing import Optional

try:
    from fastapi import UploadFile
except ImportError:
    UploadFile = None


@pytest.fixture
def sample_audio_bytes():
    """Sample audio bytes (minimal WAV file header)"""
    # Minimal WAV file header (44 bytes)
    return (
        b'RIFF'  # ChunkID
        b'\x24\x00\x00\x00'  # ChunkSize (36)
        b'WAVE'  # Format
        b'fmt '  # Subchunk1ID
        b'\x10\x00\x00\x00'  # Subchunk1Size (16)
        b'\x01\x00'  # AudioFormat (1 = PCM)
        b'\x01\x00'  # NumChannels (1 = mono)
        b'\x44\xac\x00\x00'  # SampleRate (44100)
        b'\x88\x58\x01\x00'  # ByteRate
        b'\x02\x00'  # BlockAlign
        b'\x10\x00'  # BitsPerSample (16)
        b'data'  # Subchunk2ID
        b'\x00\x00\x00\x00'  # Subchunk2Size (0 for empty)
    )


@pytest.fixture
def mock_upload_file(sample_audio_bytes):
    """Mock UploadFile object"""
    if UploadFile is None:
        pytest.skip("fastapi not installed")
    
    file_obj = io.BytesIO(sample_audio_bytes)
    file_obj.seek(0)
    
    # Create a mock UploadFile-like object
    class MockUploadFile:
        def __init__(self, filename: str, file_obj, content_type: str):
            self.filename = filename
            self.file = file_obj
            self.content_type = content_type
    
    return MockUploadFile(
        filename="test_audio.wav",
        file_obj=file_obj,
        content_type="audio/wav"
    )


@pytest.fixture
def mock_config():
    """Mock configuration for BlazeSTTExtension"""
    return {
        "api_url": "http://localhost:8000",
        "api_key": "test-api-key",
        "language": "vi",
        "enable_segments": False,
        "enable_refinement": False,
        "timeout": 3600,
    }


@pytest.fixture
def mock_api_response_completed():
    """Mock API response for completed transcription"""
    return {
        "job_status": "completed",
        "result": {
            "status_code": 200,
            "error": "",
            "data": {
                "transcription": "Xin chào, đây là test transcription",
                "is_successful": True,
            }
        }
    }


@pytest.fixture
def mock_api_response_processing():
    """Mock API response for processing job"""
    return {
        "job_id": "test-job-id-123",
        "job_status": "processing",
    }


@pytest.fixture
def mock_api_response_job_status():
    """Mock API response for get_job_status"""
    return {
        "job_id": "test-job-id-123",
        "job_status": "completed",
        "result": {
            "status_code": 200,
            "error": "",
            "data": {
                "transcription": "Xin chào, đây là test transcription",
                "is_successful": True,
            }
        }
    }

