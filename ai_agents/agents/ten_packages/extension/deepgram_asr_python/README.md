# Deepgram Nova ASR Extension

## Configuration

### Configuration Parameters

All parameters are configured through the `params` object:

### nova model

```json
{
    "params": {
        "key": "${env:DEEPGRAM_API_KEY}",
        "url": "wss://api.deepgram.com/v1/listen",
        "model": "nova-3",
        "language": "en-US",
        "sample_rate": 16000,
        "encoding": "linear16",
        "interim_results": true,
        "punctuate": true,
        "keep_alive": true
    }
}
```

### flux model

```json
{
    "params": {
        "key": "${env:DEEPGRAM_API_KEY}",
        "url": "wss://api.deepgram.com/v2/listen",
        "model": "flux-general-en",
        "sample_rate": 16000,
        "encoding": "linear16",
        "eager_eot_threshold": 0.6,
        "eot_threshold": 0.8,
        "eot_timeout_ms": 700
    }
}
```