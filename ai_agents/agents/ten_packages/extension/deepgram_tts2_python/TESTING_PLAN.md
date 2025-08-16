# Deepgram TTS2 Extension - Comprehensive Testing Plan

## 🎯 Testing Levels Overview

### 1. ✅ Basic Extension Test (COMPLETED)
**Command**: `task test-extension EXTENSION=agents/ten_packages/extension/deepgram_tts_python`

**What it tests**:
- ✅ Addon registration and loading
- ✅ Extension lifecycle (init, start, stop, deinit)
- ✅ Configuration parsing (BaseConfig)
- ✅ TTS2 interface compliance
- ✅ Error handling without API key

**Status**: ✅ PASSING (24.08s)

### 2. 🎯 Functional TTS Test (NEEDED)
**Purpose**: Test actual TTS functionality with real API calls

**Test Cases**:
- **API Connectivity**: Valid API key, WebSocket connection
- **Audio Generation**: Text → Audio conversion
- **TTS2 Interface**: `request_tts()` method functionality
- **Audio Quality**: Sample rate, encoding, format validation
- **Performance**: TTFB (Time To First Byte), latency metrics
- **Error Handling**: Invalid API key, network issues, malformed requests

**Implementation**: Create `test_functional.py`

### 3. 🔄 TTS-STT Round-trip Test (NEEDED)
**Purpose**: End-to-end accuracy testing

**Workflow**: Text → Deepgram TTS → Audio → Azure STT → Text → Accuracy

**Test Cases**:
- **Simple sentences**: "Hello, this is a test."
- **Complex sentences**: Technical terms, numbers, punctuation
- **Multiple voices**: Different Deepgram voice models
- **Accuracy metrics**: Word Error Rate (WER), Character Error Rate (CER)
- **Performance**: Total round-trip time

**Implementation**: Create `test_tts_stt_roundtrip.py`

### 4. 🏗️ Integration Test (NEEDED)
**Purpose**: Test in realistic multi-extension scenarios

**Test Cases**:
- **With ASR**: Speech → Text → TTS → Audio pipeline
- **With LLM**: Text → LLM → Enhanced Text → TTS → Audio
- **Load Testing**: Multiple concurrent TTS requests
- **Resource Management**: Memory usage, connection pooling
- **Failover**: REST API fallback when WebSocket fails

**Implementation**: Add to `/app/agents/integration_tests/`

## 🛠️ Implementation Plan

### Phase 1: Functional Testing
```python
# tests/test_functional.py
class TestDeepgramTTSFunctional:
    async def test_api_connection_with_valid_key(self):
        """Test WebSocket connection with valid API key"""
        
    async def test_text_to_audio_conversion(self):
        """Test actual TTS conversion"""
        
    async def test_tts2_interface_compliance(self):
        """Test TTS2 methods work correctly"""
        
    async def test_audio_quality_validation(self):
        """Validate audio format, sample rate, etc."""
        
    async def test_performance_metrics(self):
        """Measure TTFB and latency"""
```

### Phase 2: Round-trip Testing
```python
# tests/test_tts_stt_roundtrip.py
class TestTTSSTTRoundtrip:
    async def test_simple_sentences(self):
        """Test accuracy with simple sentences"""
        
    async def test_complex_sentences(self):
        """Test with technical terms, numbers"""
        
    async def test_multiple_voices(self):
        """Test different Deepgram voice models"""
        
    async def test_accuracy_metrics(self):
        """Calculate WER and CER"""
```

### Phase 3: Integration Testing
```python
# integration_tests/deepgram_tts/test_integration.py
class TestDeepgramTTSIntegration:
    async def test_with_asr_pipeline(self):
        """Test Speech → ASR → TTS pipeline"""
        
    async def test_with_llm_pipeline(self):
        """Test Text → LLM → TTS pipeline"""
        
    async def test_load_performance(self):
        """Test multiple concurrent requests"""
        
    async def test_failover_scenarios(self):
        """Test WebSocket → REST fallback"""
```

## 📊 Success Criteria

### Basic Extension Test
- ✅ Extension lifecycle completes successfully
- ✅ No import errors or configuration issues
- ✅ Graceful handling of missing API key

### Functional Test
- 🎯 WebSocket connection established with valid API key
- 🎯 Audio generated for test sentences (>1KB audio data)
- 🎯 TTS2 interface methods work correctly
- 🎯 TTFB < 500ms for first audio chunk
- 🎯 Audio format matches configuration (24kHz, linear16)

### Round-trip Test
- 🎯 Accuracy > 90% for simple sentences
- 🎯 Accuracy > 80% for complex sentences
- 🎯 WER < 10% for standard test cases
- 🎯 Total round-trip time < 3 seconds

### Integration Test
- 🎯 Works correctly with other TEN Framework extensions
- 🎯 Handles concurrent requests (>10 simultaneous)
- 🎯 Memory usage stable under load
- 🎯 Failover mechanisms work correctly

## 🚀 Running Tests

### All Tests
```bash
# Run all extension tests
task test

# Run specific extension test
task test-extension EXTENSION=agents/ten_packages/extension/deepgram_tts_python

# Run with specific test cases
task test-extension EXTENSION=agents/ten_packages/extension/deepgram_tts_python -- -k "test_functional"
```

### Individual Test Suites
```bash
# Basic extension test (current)
cd agents/ten_packages/extension/deepgram_tts_python
./tests/bin/start

# Functional test (to be implemented)
cd agents/ten_packages/extension/deepgram_tts_python
pytest tests/test_functional.py -v

# Round-trip test (to be implemented)
cd agents/ten_packages/extension/deepgram_tts_python
python tests/test_tts_stt_roundtrip.py

# Integration test (to be implemented)
cd agents/integration_tests/deepgram_tts
./tests/bin/start
```

## 🔧 Environment Setup

### Required API Keys
```bash
export DEEPGRAM_API_KEY="your_deepgram_api_key"
export AZURE_SPEECH_KEY="your_azure_speech_key"
export AZURE_SPEECH_REGION="eastus"
```

### Dependencies
```bash
pip install azure-cognitiveservices-speech
pip install pytest-asyncio
pip install pytest-benchmark
```

## 📈 Current Status

| Test Level | Status | Coverage | Notes |
|------------|--------|----------|-------|
| Basic Extension | ✅ PASSING | 100% | Lifecycle, config, TTS2 interface |
| Functional | ⏳ PENDING | 0% | Needs API key and implementation |
| Round-trip | ⏳ PENDING | 0% | Needs Azure STT integration |
| Integration | ⏳ PENDING | 0% | Needs multi-extension setup |

## 🎯 Next Steps

1. **✅ COMPLETED**: Basic extension test passing
2. **🎯 NEXT**: Implement functional testing with real API calls
3. **🔄 THEN**: Add TTS-STT round-trip accuracy testing
4. **🏗️ FINALLY**: Integration testing with other extensions

This comprehensive testing approach ensures our Deepgram TTS2 extension is production-ready and meets TEN Framework quality standards.
