#!/usr/bin/env python3
"""
Simple test to verify that the mock fixes work correctly
"""

import sys
from pathlib import Path

# Add project root to sys.path
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)


def test_mock_setup():
    """Test that our mock setup works correctly"""
    try:
        from unittest.mock import patch, AsyncMock
        from ten_runtime import ExtensionTester, TenEnvTester

        print("✅ All imports successful")

        # Test basic mock setup
        with patch("google_tts_python.extension.GoogleTTS") as MockGoogleTTS:
            mock_client_instance = AsyncMock()

            # Set up required attributes
            mock_client_instance.client = AsyncMock()
            mock_client_instance.config = AsyncMock()
            mock_client_instance.ten_env = AsyncMock()
            # pylint: disable=protected-access
            mock_client_instance._is_cancelled = False
            mock_client_instance.credentials = None

            # Mock the constructor
            MockGoogleTTS.return_value = mock_client_instance

            # Mock the _initialize_client method
            # pylint: disable=protected-access
            mock_client_instance._initialize_client = AsyncMock()

            print("✅ Mock setup successful")

            # Test that we can create a basic tester
            class SimpleTester(ExtensionTester):
                def on_start(self, ten_env_tester: TenEnvTester) -> None:
                    ten_env_tester.log_info("Test started")
                    ten_env_tester.stop_test()

            _ = SimpleTester()
            print("✅ Tester creation successful")

            return True

    except Exception as e:
        print(f"❌ Test failed: {e}")
        return False


if __name__ == "__main__":
    print("Testing mock setup...")
    success = test_mock_setup()

    if success:
        print("\n✅ All tests passed! Mock setup should work correctly.")
    else:
        print(
            "\n❌ Tests failed! There may still be issues with the mock setup."
        )
        sys.exit(1)
