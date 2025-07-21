# Main Control Extension

The Main Control extension is a central coordinator for the TEN Agent's star topology architecture. It manages the flow between ASR, LLM, and TTS components, making intelligent decisions about when to trigger each component.

## Overview

This extension replaces the linear pipeline topology with a star topology where:
- ASR results are processed by main_control
- main_control decides when to trigger LLM
- LLM responses are processed by main_control  
- main_control decides when to trigger TTS
- Interrupts are handled centrally

## Features

- **Centralized Decision Making**: Controls when to trigger LLM and TTS based on input
- **State Management**: Tracks conversation state throughout the interaction
- **Interrupt Handling**: Manages flush commands for user interruptions
- **Intelligent Routing**: Routes messages between components based on conversation state

## API

### Input Data
- `asr_result`: Speech recognition results from ASR extensions
- `text_data`: Responses from LLM extensions

### Output Data  
- `text_data`: Text data sent to LLM or TTS extensions

### Output Commands
- `flush`: Interrupt/flush commands sent to LLM and TTS

## Decision Logic

1. **ASR Processing**: Only triggers LLM when final transcription is received and system is idle
2. **LLM Processing**: Triggers TTS when LLM response is available (including greeting messages)
3. **Interrupt Handling**: Resets state and forwards flush to all components
4. **Greeting Messages**: Handles LLM greeting messages on user join without requiring pending state

## State Management

The extension maintains conversation state:
- `idle`: Ready for new input
- `processing_llm`: Waiting for LLM response
- `processing_tts`: TTS is generating speech

## Usage

Add the extension to your agent configuration:

```json
{
  "type": "extension",
  "name": "main_control",
  "addon": "main_control_python",
  "extension_group": "control",
  "property": {}
}
```

Configure connections to route through main_control:
- STT → main_control
- main_control → LLM → main_control  
- main_control → TTS