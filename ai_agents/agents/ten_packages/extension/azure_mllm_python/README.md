# azure_v2v_python

An extension for integrating Azure's Next Generation of **Multimodal** AI into your application, providing configurable AI-driven features such as conversational agents, task automation, and tool integration.

## API

Refer to `api` definition in [manifest.json] and default values in [property.json](property.json).

<!-- Additional API.md can be referred to if extra introduction needed -->

| **Property**               | **Type**   | **Description**                           |
|----------------------------|------------|-------------------------------------------|
| `api_key`                   | `string`   | Azure AI Foundry api key                 |
| `temperature`               | `float64`  | Sampling temperature, higher values mean more randomness |
| `model`                     | `string`   | `gpt-4o` or `gpt-4o-realtime-preview`, for details check [azure docs](https://learn.microsoft.com/en-us/azure/ai-services/speech-service/voice-live#supported-models-and-regions)   |
| `base_url`                 | `string`   | Base URI for your AI Foundry deployment               |
| `max_tokens`                | `int64`    | Maximum number of tokens to generate      |
| `prompt`                    | `string`   | Default system message to send to the model       |
| `server_vad`                | `bool`     | Flag to enable or disable server vad of OpenAI |
| `language`                  | `string`   | Language that model responds, such as `en-US`, `zh-CN`, etc |
| `dump`                      | `bool`     | Flag to enable or disable audio dump for debugging purpose  |
| `voice_name`               | `string`   | Name of the voice to use for speech synthesis |
| `voice_type`               | `string`   | Type of the voice, such as `azure-standard` or `azure-custom` |
| `voice_temperature`        | `float64`  | Temperature for voice synthesis, higher values mean more variation |
| `voice_endpoint`           | `string`   | Endpoint for the voice synthesis service, only use when using custom voice |
| `input_audio_echo_cancellation` | `bool`     | Flag to enable or disable echo cancellation for input audio |
| `input_audio_noise_reduction` | `bool`     | Flag to enable or disable noise reduction for input audio |

### Data Out

| **Name**       | **Property** | **Type**   | **Description**               |
|----------------|--------------|------------|-------------------------------|
| `text_data`    | `text`       | `string`   | Outgoing text data             |

### Command Out

| **Name**       | **Description**                             |
|----------------|---------------------------------------------|
| `flush`        | Response after flushing the current state    |

### Audio Frame In

| **Name**         | **Description**                           |
|------------------|-------------------------------------------|
| `pcm_frame`      | Audio frame input for voice processing    |

### Audio Frame Out

| **Name**         | **Description**                           |
|------------------|-------------------------------------------|
| `pcm_frame`    | Audio frame output after voice processing    |
