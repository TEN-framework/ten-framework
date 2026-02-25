# Oracle ASR Extension

Oracle Cloud Infrastructure (OCI) Speech Realtime ASR extension for the TEN Framework.

## Configuration

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| tenancy | string | Yes | | OCI tenancy OCID |
| user | string | Yes | | OCI user OCID |
| fingerprint | string | Yes | | API key fingerprint |
| key_file | string | Yes | | Path to the PEM private key file |
| compartment_id | string | Yes | | OCI compartment OCID |
| region | string | No | us-ashburn-1 | OCI region identifier |
| language | string | No | en-US | Language code for recognition |
| sample_rate | int | No | 16000 | Audio sample rate in Hz |
| final_silence_threshold_in_ms | int | No | 2000 | Silence threshold for final results |
| partial_silence_threshold_in_ms | int | No | 0 | Silence threshold for partial results |
| stabilize_partial_results | string | No | NONE | Partial result stabilization mode |
| punctuation | string | No | NONE | Punctuation mode |
| model_domain | string | No | GENERIC | Model domain |

## Environment Variables

Set OCI credentials via environment variables:

- `OCI_TENANCY`
- `OCI_USER`
- `OCI_FINGERPRINT`
- `OCI_KEY_FILE`
- `OCI_COMPARTMENT_ID`
- `OCI_REGION` (optional, defaults to `us-ashburn-1`)
