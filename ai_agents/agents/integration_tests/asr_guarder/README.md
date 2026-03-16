# ASR Guarder Integration Test

This test framework verifies that ASR extensions establish connections after startup and process real audio files correctly.
It supports multiple ASR backends (Azure ASR, Oracle ASR, etc.) through parameterized configuration.

## Supported ASR Extensions

| Extension | Parameter | Required Environment Variables |
|-----------|-----------|-------------------------------|
| Azure ASR | `azure_asr_python` | `AZURE_ASR_API_KEY`, `AZURE_ASR_REGION` |
| Oracle ASR | `oracle_asr_python` | `OCI_TENANCY`, `OCI_USER`, `OCI_FINGERPRINT`, `OCI_KEY_FILE`, `OCI_COMPARTMENT_ID`, `OCI_REGION` |

## Environment Variables

### Azure ASR

```bash
export AZURE_ASR_API_KEY=your_azure_api_key_here
export AZURE_ASR_REGION=eastus
```

### Oracle ASR

```bash
export OCI_TENANCY=your_tenancy_ocid
export OCI_USER=your_user_ocid
export OCI_FINGERPRINT=your_api_key_fingerprint
export OCI_KEY_FILE=/path/to/your/oci_api_key.pem
export OCI_COMPARTMENT_ID=your_compartment_ocid
export OCI_REGION=us-ashburn-1
```

Or create a `.env` file in the project root with the corresponding variables.

## Audio File

The test uses real PCM audio files:
- **File**: `tests/test_data/16k_en_us.pcm` (English), `tests/test_data/16k_zh_cn.pcm` (Chinese)
- **Format**: 16-bit PCM, 16kHz sample rate, mono
- **Content**: "hello world" in English / Chinese speech

## Running the Tests

### Azure ASR (default)

```bash
task asr-guarder-test
# or explicitly:
task asr-guarder-test EXTENSION=azure_asr_python
```

### Oracle ASR

```bash
task asr-guarder-test EXTENSION=oracle_asr_python
```

### Running a specific test

```bash
task asr-guarder-test EXTENSION=oracle_asr_python -- -k test_connection_timing
```

## Test Cases

| Test | Description |
|------|-------------|
| `test_connection_timing` | Verifies ASR extension establishes connection and processes audio |
| `test_asr_result` | Validates ASR result fields, language detection, and ID consistency across multiple sends |
| `test_asr_finalize` | Tests `asr_finalize` signal handling and `asr_finalize_end` response |
| `test_reconnection` | Tests reconnection mechanism with invalid credentials |
| `test_vendor_error` | Validates vendor error detection and error message format |
| `test_multi_language` | Tests English and Chinese language processing |
| `test_dump` | Verifies audio dump functionality and file content integrity |
| `test_metrics` | Validates TTFW/TTLW metrics and finalize flow |
| `test_audio_timestamp` | Validates `start_ms` and `duration_ms` timestamp accuracy |
| `test_long_duration_stream` | Tests extended streaming (>5 min) without timeout errors (skipped by default) |

## Expected Behavior

The test will:
1. Start the ASR extension with the specified config
2. Read and send real PCM audio frames from the test file
3. Verify the extension connects to the ASR service
4. Handle connection errors gracefully (for invalid credential tests)
5. Validate ASR result structure and content

### Authentication Error (Expected for invalid config tests)

When using invalid credentials, you'll see authentication errors.
This is expected behavior and indicates the error handling is working correctly.

## Audio Processing Details

- **Chunk Size**: 320 bytes per frame
- **Sleep Interval**: 0.01 seconds between frames
- **Audio Format**: 16-bit PCM, 16kHz, mono
