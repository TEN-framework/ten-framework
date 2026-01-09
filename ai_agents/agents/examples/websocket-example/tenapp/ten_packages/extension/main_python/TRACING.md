# TEN Agent Tracing with OpenTelemetry

This document explains how to use the built-in OpenTelemetry tracing integration for the TEN Voice Agent.

## Overview

The tracing integration provides observability for the voice agent pipeline, similar to [LiveKit Agent Insights](https://docs.livekit.io/deploy/observability/insights/). It captures:

- **Session spans** - The entire agent session lifecycle
- **User turn spans** - Each user-agent interaction turn
- **LLM spans** - LLM inference calls
- **TTS spans** - Text-to-speech synthesis
- **Tool call spans** - Function/tool execution

## Setup

### 1. Install Dependencies

Add the following Python packages to your environment:

```bash
pip install opentelemetry-api opentelemetry-sdk opentelemetry-exporter-otlp-proto-grpc
```

Or add to your `requirements.txt`:

```
opentelemetry-api>=1.20.0
opentelemetry-sdk>=1.20.0
opentelemetry-exporter-otlp-proto-grpc>=1.20.0
```

### 2. Start a Tracing Backend

The easiest way is to use Jaeger with Docker:

```bash
docker run -d --name jaeger \
  -e COLLECTOR_OTLP_ENABLED=true \
  -p 16686:16686 \
  -p 4317:4317 \
  -p 4318:4318 \
  jaegertracing/all-in-one:latest
```

Or use the provided docker-compose file:

```bash
cd ten_packages/extension/main_python
docker-compose -f docker-compose.tracing.yml up -d
```

### 3. Configure Tracing

Set environment variables:

```bash
export TEN_TRACING_ENABLED=true
export TEN_OTLP_ENDPOINT=http://localhost:4317
export TEN_SERVICE_NAME=ten-voice-agent
```

Or configure directly in `extension.py`:

```python
tracing_config = TracingConfig(
    enabled=True,
    otlp_endpoint="http://localhost:4317",
    service_name="ten-voice-agent",
)
```

### 4. View Traces

Open Jaeger UI at http://localhost:16686 and select the `ten-voice-agent` service.

## Trace Structure

```
agent_session (root)
├── user_turn (turn_id: 1)
│   ├── llm_node (request_id: xxx)
│   │   └── tool_call (name: weather_api)
│   └── tts_node (request_id: tts-1)
├── user_turn (turn_id: 2)
│   ├── llm_node (request_id: yyy)
│   └── tts_node (request_id: tts-2)
└── ...
```

## Span Attributes

### Session Span
- `ten.session_id` - Session identifier
- `ten.agent_name` - Agent name

### Turn Span
- `ten.turn_id` - Turn number
- `ten.user_input` - User's speech text
- `ten.interrupted` - Whether turn was interrupted

### LLM Span
- `ten.llm.request_id` - LLM request ID
- `ten.llm.response_text` - LLM response (truncated)
- `gen_ai.request.model` - Model name (if available)
- `gen_ai.usage.input_tokens` - Input token count
- `gen_ai.usage.output_tokens` - Output token count

### TTS Span
- `ten.tts.input_text` - Text sent to TTS
- `ten.tts.request_id` - TTS request ID

### Tool Call Span
- `ten.tool.name` - Tool/function name
- `ten.tool.arguments` - Arguments JSON
- `ten.tool.output` - Tool output
- `ten.tool.is_error` - Whether tool failed

## Events

The following events are recorded on spans:

- `user_joined` - User joined the session
- `user_left` - User left the session
- `tool_registered` - A tool was registered
- `llm_response_complete` - LLM finished generating
- `interrupted` - Turn was interrupted by user

## Integration with Grafana

For production use, you can send traces to Grafana Tempo:

```python
tracing_config = TracingConfig(
    enabled=True,
    otlp_endpoint="https://tempo.your-domain.com:4317",
    service_name="ten-voice-agent",
)
```

## Disabling Tracing

Set the environment variable:

```bash
export TEN_TRACING_ENABLED=false
```

Or in code:

```python
tracing_config = TracingConfig(enabled=False)
```

## Programmatic Usage

You can also use the tracer programmatically in your extension code:

```python
from .tracing import get_tracer

tracer = get_tracer()

# Add custom events
tracer.add_session_event("custom_event", key="value")

# Add custom spans
async with tracer.trace_llm(request_id="custom-id"):
    # Your LLM code
    pass
```
