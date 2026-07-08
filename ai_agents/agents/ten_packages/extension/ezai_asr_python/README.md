# EZAI ASR Extension

## Configuration

### Configuration Parameters

All parameters are configured through the `params` object:

### nova model

```json
{
    "params": {
        "key": "${env:EZAI_API_KEY}",
        "url": "wss://qwlk.ezai-k8s.freeddns.org/v1/listen",
        "language": "en-US",
        "sample_rate": 16000,
        "encoding": "linear16",
        "interim_results": true,
        "punctuate": true,
        "keep_alive": true
    }
}
```