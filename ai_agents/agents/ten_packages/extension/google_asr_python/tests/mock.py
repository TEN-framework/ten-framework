import pytest
from unittest.mock import MagicMock, AsyncMock


@pytest.fixture
def patch_google_asr():
    """
    Pytest fixture to patch the Google Cloud Speech client.
    This fixture mocks the SpeechAsyncClient to prevent actual network calls
    and allow simulating responses from the Google ASR service.
    """

    # Mock the SpeechAsyncClient instance that will be created
    recognizer_instance = MagicMock()

    # Mock the main client class
    speech_client_mock = MagicMock()
    speech_client_mock.return_value = recognizer_instance

    # Since SpeechAsyncClient is used with 'from google.cloud import speech_v2 as speech',
    # we need to patch 'speech.SpeechAsyncClient'.
    # However, for simplicity and to avoid complex patching issues with namespaces,
    # we will inject this mock manually in the test.
    # This fixture will provide the necessary mocked objects.

    # We will also mock the from_service_account_file class method
    from_service_account_mock = MagicMock()
    from_service_account_mock.return_value = recognizer_instance

    return {
        "client_class": speech_client_mock,
        "recognizer_instance": recognizer_instance,
        "from_service_account_file": from_service_account_mock,
    }
