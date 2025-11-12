# Amazon Polly TTS Python Extension

This extension provides text-to-speech functionality using the Amazon Polly TTS API.

## Features

- Streaming audio synthesis using Amazon Polly's TTS API
- Supports multiple voices and engines (standard, neural)
- Configurable sample rate and language
- Built-in retry mechanism for robust connections
- AWS credentials support

## Configuration

The extension can be configured through your property.json:

```json
{
  "params": {
    "aws_access_key_id": "${env:AWS_TTS_ACCESS_KEY_ID}",
    "aws_secret_access_key": "${env:AWS_TTS_SECRET_ACCESS_KEY}",
    "engine": "neural",
    "voice": "Joanna",
    "sample_rate": "16000",
    "lang_code": "en-US"
  }
}
```

### Configuration Options

**Top-level properties:**
- `dump` (optional): Enable audio dumping for debugging (default: false)
- `dump_path` (optional): Path for audio dumps (default: extension directory + "polly_tts_in.pcm")

**Parameters inside `params` object:**
- `aws_access_key_id` (required): AWS access key ID
- `aws_secret_access_key` (required): AWS secret access key
- `aws_session_token` (optional): AWS session token for temporary credentials
- `region_name` (optional): AWS region (default: uses AWS default)
- `profile_name` (optional): AWS profile name
- `aws_account_id` (optional): AWS account ID
- `engine` (optional): TTS engine - "standard" or "neural" (default: "neural")
- `voice` (optional): Voice ID to use (default: "Joanna")
- `sample_rate` (optional): Audio sample rate - "8000", "16000", "22050", "24000" (default: "16000")
- `lang_code` (optional): Language code (default: "en-US")
- `audio_format` (optional): Output audio format (default: "pcm")

## AWS Credentials

You can provide AWS credentials in multiple ways:
1. Directly in the `params` object (aws_access_key_id, aws_secret_access_key)
2. Environment variables (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY)
3. AWS credentials file (~/.aws/credentials)
4. IAM role (when running on EC2 or ECS)

See [AWS Boto3 credentials documentation](https://boto3.amazonaws.com/v1/documentation/api/latest/guide/credentials.html) for more details.

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- `extension.py`: Main extension class inheriting from `AsyncTTS2HttpExtension`
- `polly_tts.py`: Client implementation (`PollyTTSClient`) with streaming support
- `config.py`: Configuration model extending `AsyncTTS2HttpConfig`

The configuration uses a `params` dict to encapsulate all TTS-specific parameters, keeping the top level clean with only framework-related properties.
