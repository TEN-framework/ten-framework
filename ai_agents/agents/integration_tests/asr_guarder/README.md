# Azure ASR Connection Timing Test

This test verifies that the Azure ASR extension establishes connection after startup and processes real audio files.

## Environment Variables

Before running the test, you need to set the following environment variables:

```bash
# Azure Cognitive Services API Key
export AZURE_ASR_API_KEY=your_azure_api_key_here

# Azure Region (e.g., eastus, westus, eastasia, etc.)
export AZURE_ASR_REGION=eastus
```

Or create a `.env` file in the project root:

```bash
# .env file
AZURE_ASR_API_KEY=your_azure_api_key_here
AZURE_ASR_REGION=eastus
```

## Audio File

The test uses a real PCM audio file containing "hello world" in English:
- **File**: `tests/test_data/16k_en_us_helloworld.pcm`
- **Format**: 16-bit PCM, 16kHz sample rate
- **Content**: "hello world" in English
- **Size**: ~29KB

## Running the Test

```bash
# Run the test
bash tests/bin/start tests/test_azure_asr_connection_timing.py::test_azure_asr_connection_timing --extension_name=azure_asr_python
```

## Test Purpose

This test verifies:
1. Azure ASR extension establishes connection after startup
2. Extension handles connection errors properly
3. Real audio file processing works correctly
4. Audio frame sending with real PCM data
5. ASR result validation is functional

## Expected Behavior

The test will:
1. Start the Azure ASR extension
2. Read and send real PCM audio frames from the test file
3. Verify the extension attempts to connect to Azure services
4. Handle connection errors gracefully (due to invalid API key in test)
5. Validate the test framework functionality with real audio data

### Authentication Error (Expected)
When using the default `test_key`, you'll see an authentication error:
```
Authentication error (401). Please check subscription information and region name.
```
This is expected behavior and indicates the test framework is working correctly.

## Audio Processing Details

- **Chunk Size**: 320 bytes per frame
- **Sleep Interval**: 0.01 seconds between frames
- **Audio Format**: 16-bit PCM, 16kHz, mono
- **Expected Recognition**: "hello world" in English