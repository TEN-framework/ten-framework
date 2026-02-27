# Oracle TTS Extension

Oracle Cloud Infrastructure (OCI) Speech TTS extension for the TEN Framework.

## Configuration

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| tenancy | string | Yes | | OCI tenancy OCID |
| user | string | Yes | | OCI user OCID |
| fingerprint | string | Yes | | API key fingerprint |
| key_file | string | Yes | | Path to the PEM private key file |
| compartment_id | string | Yes | | OCI compartment OCID |
| region | string | No | us-phoenix-1 | OCI region identifier |
| model_name | string | No | TTS_2_NATURAL | TTS model (`TTS_1_STANDARD` or `TTS_2_NATURAL`) |
| voice_id | string | No | Annabelle | Voice identifier |
| language_code | string | No | en-US | Language code for synthesis |
| sample_rate | int | No | 16000 | Audio sample rate in Hz |
| output_format | string | No | PCM | Audio output format |

## Environment Variables

Set OCI credentials via environment variables:

- `OCI_TENANCY`
- `OCI_USER`
- `OCI_FINGERPRINT`
- `OCI_KEY_FILE`
- `OCI_COMPARTMENT_ID`
- `OCI_REGION` (optional, defaults to `us-phoenix-1`)
