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
def extension(mock_ten_env):
    ext = BytedanceASRLLMExtension("test_extension")
    ext.ten_env = mock_ten_env
    ext.connected = True
    ext.client = MagicMock()
    ext.client.connected = False
    ext.client.send_audio = AsyncMock()
    return ext


@pytest.mark.asyncio
async def test_handle_audio_frame_buffers_when_client_state_is_disconnected(
    extension, mock_ten_env
):
    frame = MagicMock()
    frame.get_buf.return_value = b"\x00\x01"

    await extension._handle_audio_frame(mock_ten_env, frame)

    assert extension.is_connected() is False
    assert extension.buffered_frames.qsize() == 1
    extension.client.send_audio.assert_not_awaited()
    mock_ten_env.log_debug.assert_called_once_with(
        "send_frame: service not connected."
    )
