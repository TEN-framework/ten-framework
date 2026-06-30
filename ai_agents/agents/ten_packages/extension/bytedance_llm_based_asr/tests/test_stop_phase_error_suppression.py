import os
import sys
import types
from unittest.mock import AsyncMock, MagicMock

import pytest


extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

package = types.ModuleType("bytedance_llm_based_asr")
package.__path__ = [extension_dir]
sys.modules["bytedance_llm_based_asr"] = package

from bytedance_llm_based_asr import config as config_module

sys.modules["bytedance_llm_based_asr.config"] = config_module

from bytedance_llm_based_asr import extension as extension_module

sys.modules["bytedance_llm_based_asr.extension"] = extension_module

from bytedance_llm_based_asr.extension import BytedanceASRLLMExtension


@pytest.fixture
def mock_ten_env():
    env = AsyncMock()
    env.log = MagicMock()
    env.log_debug = MagicMock()
    env.log_info = MagicMock()
    env.log_warn = MagicMock()
    env.log_error = MagicMock()
    return env


@pytest.fixture
def mock_frame():
    frame = MagicMock()
    frame.lock_buf.return_value = b"\x00\x01"
    return frame


@pytest.fixture
def extension(mock_ten_env):
    ext = BytedanceASRLLMExtension("test_extension")
    ext.ten_env = mock_ten_env
    ext.connected = True
    ext.client = MagicMock()
    ext.client.send_audio = AsyncMock()
    ext.send_asr_error = AsyncMock()
    return ext


@pytest.mark.asyncio
async def test_send_audio_suppresses_stop_phase_disconnect_error(
    extension, mock_ten_env, mock_frame
):
    extension.stopped = True
    extension.client.send_audio.side_effect = RuntimeError(
        "Not connected to ASR service"
    )

    result = await extension.send_audio(mock_frame, "session-1")

    assert result is False
    extension.send_asr_error.assert_not_awaited()
    mock_ten_env.log.assert_not_called()
    mock_ten_env.log_debug.assert_not_called()
    mock_frame.unlock_buf.assert_called_once_with(b"\x00\x01")


@pytest.mark.asyncio
async def test_send_audio_reports_runtime_disconnect_error(
    extension, mock_ten_env, mock_frame
):
    extension.stopped = False
    extension.client.send_audio.side_effect = RuntimeError(
        "Not connected to ASR service"
    )

    result = await extension.send_audio(mock_frame, "session-1")

    assert result is False
    extension.send_asr_error.assert_awaited_once()
    mock_ten_env.log.assert_called_once()
    mock_frame.unlock_buf.assert_called_once_with(b"\x00\x01")


@pytest.mark.asyncio
async def test_send_audio_does_not_suppress_other_stop_phase_errors(
    extension, mock_ten_env, mock_frame
):
    extension.stopped = True
    extension.client.send_audio.side_effect = ValueError("unexpected failure")

    result = await extension.send_audio(mock_frame, "session-1")

    assert result is False
    extension.send_asr_error.assert_not_awaited()
    mock_ten_env.log.assert_not_called()
    mock_ten_env.log_debug.assert_not_called()
    mock_frame.unlock_buf.assert_called_once_with(b"\x00\x01")
