# Speaker Diarization Implementation - Summary of Changes

## Issues Fixed

### 1. **Incorrect Class Name for Speaker Diarization Config**
   - **Problem**: Code was using `SpeakerDiarizationConfig` which doesn't exist in `speechmatics-python==3.0.2`
   - **Fix**: Changed to `RTSpeakerDiarizationConfig` (RT = RealTime)
   - **File**: `ai_agents/agents/ten_packages/extension/speechmatics_asr_python/asr_client.py:127`

### 2. **Unsupported Parameters**
   - **Problem**: Code was passing `speaker_sensitivity` parameter which doesn't exist in v3.0.2
   - **Fix**: Removed `speaker_sensitivity` parameter, only using `max_speakers`
   - **File**: `ai_agents/agents/ten_packages/extension/speechmatics_asr_python/asr_client.py:127-131`

### 3. **Documentation Updates**
   - **Problem**: README documented parameters that aren't supported
   - **Fix**: Updated README to clearly indicate which parameters are not functional in current version
   - **File**: `ai_agents/agents/examples/diarization/README.md`

## Current Implementation Status

### ✅ Working Features
- Speaker detection and labeling (`[S1]`, `[S2]`, etc.)
- Multi-speaker support (up to 100 speakers)
- `max_speakers` configuration parameter
- Speaker labels appear in transcript UI
- Speaker context passed to LLM
- Channel diarization support
- Combined channel and speaker diarization

### ⚠️ Limited Support (Due to Library Version)
- `speaker_sensitivity` - Parameter exists in config but not used by library v3.0.2
- `prefer_current_speaker` - Parameter exists in config but not used by library v3.0.2

## Configuration

### Supported Parameters
```json
{
  "diarization": "speaker",           // ✅ Fully supported
  "max_speakers": 10,                 // ✅ Fully supported
  "speaker_sensitivity": 0.5,         // ⚠️ Config exists but not used
  "prefer_current_speaker": false     // ⚠️ Config exists but not used
}
```

## Code Changes

### asr_client.py (Line 127-131)
```python
# Before:
speaker_diarization_config = speechmatics.models.SpeakerDiarizationConfig(
    max_speakers=self.config.max_speakers,
    speaker_sensitivity=self.config.speaker_sensitivity,
)

# After:
speaker_diarization_config = speechmatics.models.RTSpeakerDiarizationConfig(
    max_speakers=self.config.max_speakers,
)
# Note: speaker_sensitivity and prefer_current_speaker are not supported in speechmatics-python 3.0.2
```

## How Speaker Labels Work

1. **Backend (Speechmatics ASR)**:
   - Returns speaker info in metadata: `{"speaker": "S1"}`

2. **Extension (asr_client.py)**:
   - Extracts speaker from response metadata
   - Stores in `result_metadata["speaker"]`

3. **Main Control (main_python/extension.py)**:
   - Receives ASR result with speaker metadata
   - Appends label to text: `event.text + " [S1]"`
   - Sends to message collector

4. **UI (MessageList.tsx)**:
   - Displays the text with speaker labels
   - Example: "Hello [S1]"

## Testing

To verify diarization is working:
1. Open playground UI at `http://localhost:3000`
2. Start a session
3. Speak into the microphone
4. Look for `[S1]`, `[S2]` labels in the transcript (right side)
5. Have multiple people speak to see different speaker labels

## Future Improvements

To support `speaker_sensitivity` and `prefer_current_speaker`:
- Upgrade to a newer version of `speechmatics-python` that supports these parameters
- Or use the Speechmatics API directly instead of the Python library

## Dependencies

- `speechmatics-python==3.0.2` (current version with limited diarization support)
- Speechmatics API key required
- API endpoint: `wss://eu2.rt.speechmatics.com/v2`
