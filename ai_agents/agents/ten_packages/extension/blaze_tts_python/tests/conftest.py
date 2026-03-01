"""
Pytest fixtures for Blaze TTS Extension tests
"""

import pytest


@pytest.fixture
def mock_config():
    """Mock configuration for BlazeTTSExtension"""
    return {
        "api_url": "http://localhost:8000",
        "api_key": "test-api-key",
        "language": "vi",
        "speaker_id": "test-speaker-123",
        "audio_speed": 1.0,
        "audio_quality": 64,
        "timeout": 3600,
    }


@pytest.fixture
def mock_api_response_synthesize():
    """Mock API response for synthesize"""
    return {
        "job_id": "test-tts-job-123",
        "job_status": "completed",
        "audio_url": "https://example.com/audio/test-tts-job-123.mp3",
    }


@pytest.fixture
def mock_api_response_speakers():
    """Mock API response for list_speakers"""
    return {
        "list_speakers": [
            {
                "id": "speaker-1",
                "name": "Vietnamese Female",
                "language": "vi",
                "gender": "female",
            },
            {
                "id": "speaker-2",
                "name": "Vietnamese Male",
                "language": "vi",
                "gender": "male",
            },
        ]
    }


@pytest.fixture
def mock_api_response_job_info():
    """Mock API response for get_job_info"""
    return {
        "job_id": "test-tts-job-123",
        "job_status": "completed",
        "audio_url": "https://example.com/audio/test-tts-job-123.mp3",
    }
