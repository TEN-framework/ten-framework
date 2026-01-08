# Conversation Recorder Extension

Records conversation audio (user and agent) to a file and optionally uploads it to cloud storage.

## Features

- **Audio Mixing**: Mixes audio from multiple sources (user input + agent output) into a single recording
- **Multiple Storage Backends**: Local filesystem, Google Cloud Storage (GCS), or S3-compatible storage
- **Custom Filenames**: Optionally specify a custom filename or auto-generate with timestamp
- **Automatic Lifecycle**: Starts recording on user join, stops and saves on user leave

## Installation

The extension is included in the TEN Agent ten_packages. Ensure the following dependencies are installed:

```
numpy
scipy
google-cloud-storage  # Only needed for GCS storage
boto3                  # Only needed for S3 storage
```

## Configuration

### Basic Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `storage_type` | string | `"local"` | Storage backend: `"local"`, `"gcs"`, or `"s3"` |
| `file_path` | string | `"records/"` | Local directory path (for local storage) |
| `filename` | string | auto-generated | Custom filename (works with all storage modes) |
| `start_trigger` | string | `"on_user_joined"` | When to start recording: `"on_user_joined"` or `"on_start"` |
| `sample_rate` | int | `24000` | Audio sample rate in Hz |

### GCS Storage Properties

| Property | Type | Description |
|----------|------|-------------|
| `gcp_bucket_name` | string | GCS bucket name (required for GCS) |
| `gcp_project_id` | string | GCP project ID (optional if using ADC) |
| `gcp_credentials_path` | string | Path to service account JSON file |
| `gcp_upload_prefix` | string | Folder prefix in bucket (e.g., `"recordings/"`) |

### S3 Storage Properties

| Property | Type | Description |
|----------|------|-------------|
| `s3_bucket_name` | string | S3 bucket name (required for S3) |
| `s3_access_key_id` | string | AWS access key ID (optional if using IAM roles) |
| `s3_secret_access_key` | string | AWS secret access key |
| `s3_endpoint_url` | string | Custom endpoint URL (for MinIO, DigitalOcean Spaces, etc.) |
| `s3_region` | string | AWS region (e.g., `"us-east-1"`) |
| `s3_upload_prefix` | string | Folder prefix in bucket |

## Usage Examples

### Local Storage

```json
{
  "type": "extension",
  "name": "conversation_recorder",
  "addon": "conversation_recorder",
  "extension_group": "default",
  "property": {
    "storage_type": "local",
    "file_path": "records",
    "start_trigger": "on_user_joined"
  }
}
```

### Google Cloud Storage

```json
{
  "type": "extension",
  "name": "conversation_recorder",
  "addon": "conversation_recorder",
  "extension_group": "default",
  "property": {
    "storage_type": "gcs",
    "gcp_bucket_name": "my-recordings-bucket",
    "gcp_upload_prefix": "conversations/",
    "gcp_credentials_path": "/path/to/service-account.json",
    "start_trigger": "on_user_joined"
  }
}
```

### S3-Compatible Storage (AWS S3)

```json
{
  "type": "extension",
  "name": "conversation_recorder",
  "addon": "conversation_recorder",
  "extension_group": "default",
  "property": {
    "storage_type": "s3",
    "s3_bucket_name": "my-recordings-bucket",
    "s3_region": "us-east-1",
    "s3_upload_prefix": "conversations/",
    "start_trigger": "on_user_joined"
  }
}
```

### S3-Compatible Storage (MinIO)

```json
{
  "type": "extension",
  "name": "conversation_recorder",
  "addon": "conversation_recorder",
  "extension_group": "default",
  "property": {
    "storage_type": "s3",
    "s3_bucket_name": "my-recordings-bucket",
    "s3_endpoint_url": "https://minio.example.com:9000",
    "s3_access_key_id": "my-access-key",
    "s3_secret_access_key": "my-secret-key",
    "s3_upload_prefix": "conversations/"
  }
}
```

## Graph Integration

To use this extension in your TEN Agent graph, you need to:

1. **Add the node** to your graph's `nodes` array
2. **Connect audio sources** to the recorder (user audio from `streamid_adapter` and agent audio from `v2v`)
3. **Connect user events** to trigger recording start/stop

### Example Graph Configuration

```json
{
  "name": "my_graph",
  "auto_start": true,
  "graph": {
    "nodes": [
      {
        "type": "extension",
        "name": "agora_rtc",
        "addon": "agora_rtc",
        "property": { ... }
      },
      {
        "type": "extension",
        "name": "streamid_adapter",
        "addon": "streamid_adapter",
        "property": {}
      },
      {
        "type": "extension",
        "name": "v2v",
        "addon": "gemini_mllm_python",
        "property": { ... }
      },
      {
        "type": "extension",
        "name": "conversation_recorder",
        "addon": "conversation_recorder",
        "extension_group": "default",
        "property": {
          "storage_type": "local",
          "file_path": "records",
          "start_trigger": "on_user_joined"
        }
      }
    ],
    "connections": [
      {
        "extension": "streamid_adapter",
        "audio_frame": [
          {
            "name": "pcm_frame",
            "dest": [
              { "extension": "v2v" },
              { "extension": "conversation_recorder" }
            ]
          }
        ]
      },
      {
        "extension": "v2v",
        "audio_frame": [
          {
            "name": "pcm_frame",
            "dest": [
              { "extension": "conversation_recorder" }
            ]
          }
        ]
      },
      {
        "extension": "conversation_recorder",
        "cmd": [
          {
            "names": ["on_user_joined", "on_user_left"],
            "source": [
              { "extension": "agora_rtc" }
            ]
          }
        ]
      }
    ]
  }
}
```

### Connection Diagram

```
                    ┌─────────────────┐
                    │    agora_rtc    │
                    │  (user audio)   │
                    └────────┬────────┘
                             │ pcm_frame
                             ▼
                    ┌─────────────────┐
                    │ streamid_adapter│
                    └────────┬────────┘
                             │ pcm_frame
              ┌──────────────┴──────────────┐
              ▼                             ▼
        ┌─────────┐                ┌──────────────────┐
        │   v2v   │                │                  │
        │ (LLM)   │                │ conversation_    │
        └────┬────┘                │ recorder         │
             │ pcm_frame           │                  │
             └────────────────────►│ (mixes both      │
                                   │  audio sources)  │
                                   └──────────────────┘
                                            │
                                            ▼
                                   ┌──────────────────┐
                                   │  Storage Backend │
                                   │ (local/gcs/s3)   │
                                   └──────────────────┘
```

### Required Connections

1. **User audio**: Connect `pcm_frame` from `streamid_adapter` to `conversation_recorder`
2. **Agent audio**: Connect `pcm_frame` from `v2v` (LLM extension) to `conversation_recorder`
3. **User events**: Connect `on_user_joined` and `on_user_left` commands from `agora_rtc` to `conversation_recorder`

## Output

- **Local storage**: Saves WAV file to the specified `file_path` directory
- **GCS**: Uploads to `gs://{bucket}/{prefix}/{filename}.wav`
- **S3**: Uploads to `s3://{bucket}/{prefix}/{filename}.wav`

If no custom `filename` is provided, files are named with timestamp: `conversation_YYYYMMDD_HHMMSS.wav`


## Author

Written by [Roei Bracha](https://github.com/Roei-Bracha)
