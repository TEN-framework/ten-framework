# Google TTS Extension Tests

This directory contains comprehensive unit tests for the Google TTS extension, following the same pattern as the ElevenLabs TTS extension tests.

## Test Files

- `test_basic.py` - Basic functionality tests
- `test_params.py` - Parameter configuration tests  
- `test_error_msg.py` - Error message handling tests
- `test_metrics.py` - Metrics and timing tests
- `test_robustness.py` - Robustness and stress tests
- `test_error_debug.py` - Error debugging tests

## Running Tests

```bash
cd TEN-Agent/ai_agents/agents/ten_packages/extension/google_tts_python/tests
python -m pytest -v
```

## Test Configuration

All tests use mocked Google TTS client to avoid actual API calls. Tests verify:
- Core TTS functionality
- Error handling scenarios
- Configuration parameters
- Metrics and timing
- Robustness under various conditions
