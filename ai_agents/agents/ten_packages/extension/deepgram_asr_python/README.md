# Deepgram Nova ASR Extension

## Configuration

### Configuration Parameters

All parameters are configured through the `params` object:

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
