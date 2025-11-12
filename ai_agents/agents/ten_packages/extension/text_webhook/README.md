# text_webhook

A webhook extension for Ten Framework that sends text data received by an agent to an external HTTP endpoint. This extension enables integration with external services, databases, analytics platforms, or other custom systems.

**For best results** connect the message collector extension to this extension, which will allow it to receive text data from conversations and forward it to the specified webhook URL.

## Features

- **HTTP Webhook Integration**: Sends text data to a specified URL via configurable HTTP requests
- **Authentication Support**: Configure custom headers for authorization (Bearer tokens, API keys, etc.)
- **Flexible Configuration**: Customize HTTP method, timeout, and other parameters
- **Real-time Data Forwarding**: Forward text data from conversations as they happen
- **Filtering Options**: Choose to send all text data or only final (complete) messages
- **Text Aggregation**: Automatically combines related text segments from the same stream
- **Message Identification**: Includes unique message IDs and timestamps for tracking
- **Conversation Lifecycle**: Optional ability to send conversation start and end notifications
- **Message Collector Integration**: Direct forwarding of data from the message collector

## API

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).

### Configuration Properties

| Property | Type | Description | Default |
|----------|------|-------------|---------|
| `url` | string | The destination webhook URL | `""` |
| `headers` | string | JSON string of headers (e.g., `{"Authorization": "Bearer YOUR_TOKEN"}`) | `""` |
| `method` | string | HTTP method to use (POST, PUT, etc.) | `"POST"` |
| `timeout` | int | Request timeout in seconds | `10` |
| `send_final_only` | bool | When true, only sends final text data (ignores partial/interim results) | `true` |
| `data_type` | string | Type of data being sent (transcribe, raw, etc.) | `"transcribe"` |
| `send_on_close` | bool | When true, sends a conversation end notification when extension closes | `true` |
| `send_on_start` | bool | When true, sends a conversation start notification when extension starts | `true` |
| `direct_forward` | bool | When true, directly forwards data from message collector without parsing | `false` |

### Data Input

The extension listens for the following data types:

#### `text_data`
- `text`: The text content to send
- `is_final`: Whether the text is a final version
- `stream_id`: An identifier for the message stream
- `end_of_segment`: Marks the end of a text segment

#### `content_data`
- `text`: Raw content data
- `stream_id`: An identifier for the message stream
- `end_of_segment`: Marks the end of a text segment

#### `data`
- `data`: Raw data buffer (used for message collector integration)

### Commands

- `flush`: Closes the current HTTP session, clears cached text data, and acknowledges with a `flush` command

## Usage Examples

### Basic Usage

Configure the extension with a webhook URL:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming"
}
```

### With Authentication (Bearer Token)

Configure the extension with a webhook URL and an authorization header:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "headers": "{\"Authorization\": \"Bearer YOUR_TOKEN_HERE\"}"
}
```

### With API Key Authentication

Configure the extension with a webhook URL and an API key header:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "headers": "{\"X-API-Key\": \"YOUR_API_KEY_HERE\"}"
}
```

### Using a Different HTTP Method

Configure the extension to use PUT instead of POST:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "method": "PUT"
}
```

### Send Only Final Text

To filter out partial or interim text results, sending only the final, confirmed text:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "send_final_only": true
}
```

### Custom Data Type

To categorize your data with a specific type for downstream processing:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "data_type": "customer_conversation"
}
```

### Enable Conversation Lifecycle Notifications

To send special notifications when the conversation starts and ends:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "send_on_start": true,
  "send_on_close": true
}
```

### Direct Forward Mode for Message Collector

When working with the message collector extension, enable direct forwarding mode for compatibility:

```json
{
  "url": "https://your-webhook-endpoint.com/incoming",
  "direct_forward": true
}
```

## Example Webhook Payloads

### Standard Text Message

```json
{
  "text": "This is the text content from the conversation",
  "is_final": true,
  "end_of_segment": true,
  "stream_id": 123,
  "message_id": "a1b2c3d4",
  "conversation_id": "ef5678gh",
  "data_type": "transcribe",
  "text_ts": 1625145600000
}
```

### Conversation Start Notification

When `send_on_start` is enabled, a special notification is sent when the extension starts:

```json
{
  "text": "",
  "is_final": true,
  "end_of_segment": false,
  "stream_id": 0,
  "message_id": "i9j0k1l2",
  "conversation_id": "ef5678gh",
  "data_type": "transcribe",
  "text_ts": 1625145600000,
  "conversation_start": true
}
```

### Conversation End Notification

When `send_on_close` is enabled, a special notification is sent when the extension is shutting down:

```json
{
  "text": "",
  "is_final": true,
  "end_of_segment": true,
  "stream_id": 0,
  "message_id": "m3n4o5p6",
  "conversation_id": "ef5678gh",
  "data_type": "transcribe",
  "text_ts": 1625145600000,
  "conversation_end": true
}
```

### Direct Forward Payload

When `direct_forward` is enabled, raw data from the message collector is forwarded:

```json
{
  "text": "Raw message collector data here",
  "is_final": true,
  "message_id": "q7r8s9t0",
  "conversation_id": "ef5678gh",
  "timestamp": 1625145600000,
  "direct_forward": true
}
```

### Payload Fields Explained

- `text`: The actual text content
- `is_final`: Whether this is a final (confirmed) text segment
- `end_of_segment`: Whether this is the end of a logical text segment
- `stream_id`: Identifier for the message stream (useful for tracking multi-part messages)
- `message_id`: Unique identifier for this specific message
- `conversation_id`: Unique identifier for the entire conversation session
- `data_type`: Type of data being sent (transcribe or raw)
- `text_ts` or `timestamp`: Timestamp in milliseconds when the message was processed
- `conversation_start`: (Only in start notifications) Flag indicating conversation has started
- `conversation_end`: (Only in end notifications) Flag indicating conversation has ended
- `direct_forward`: (Only in direct forwarded messages) Flag indicating message was directly forwarded

## Message Collector Integration

The extension provides special handling for data coming from the message collector extension:

1. **Standard Mode**: Attempts to parse the message collector's formatted data to extract text content
2. **Direct Forward Mode**: When `direct_forward=true`, bypasses parsing and directly forwards the raw data

This flexibility allows for working with different message collector configurations:

- Use standard mode when you want the webhook extension to extract and format the text
- Use direct forward mode when you want the raw message collector data to be sent to your webhook

## Text Caching Behavior

The extension maintains a cache of text by `stream_id`, which works as follows:

1. When text is received with `is_final=true` but `end_of_segment=false`:
   - The text is cached under its `stream_id`
   - Subsequent text with the same `stream_id` will be appended

2. When text is received with `end_of_segment=true`:
   - Any cached text for that `stream_id` is prepended to the current text
   - The combined text is sent to the webhook
   - The cache entry for that `stream_id` is cleared

This ensures that related text segments are properly combined before being sent.

## Conversation Lifecycle Behavior

The extension provides notifications at key points in a conversation's lifecycle:

### Conversation Start

When the `send_on_start` option is enabled:

1. A special notification is sent to the webhook when the extension starts
2. This notification includes a `conversation_start: true` flag
3. A unique `conversation_id` is generated and included in all messages
4. Applications can use this notification to:
   - Create new conversation records in a database
   - Initialize resources for the conversation
   - Start tracking conversation metrics
   - Signal to other systems that a new conversation has begun

### Conversation End

When the `send_on_close` option is enabled:

1. A special notification is sent to the webhook when the extension is shutting down
2. This notification includes a `conversation_end: true` flag
3. The same `conversation_id` is included to link it to the start notification
4. Applications can use this notification to:
   - Close or finalize conversation records
   - Trigger post-conversation workflows
   - Update conversation status in databases
   - Signal to other systems that no more data will be sent for this session

## Integration Examples

### CRM Integration

Configure the extension to send conversation data to your CRM system:

```json
{
  "url": "https://your-crm.com/api/conversations",
  "headers": "{\"Authorization\": \"Bearer YOUR_CRM_API_TOKEN\", \"Content-Type\": \"application/json\"}",
  "send_final_only": true,
  "data_type": "customer_conversation",
  "send_on_start": true,
  "send_on_close": true
}
```

### Analytics Integration

Send conversation data to an analytics platform:

```json
{
  "url": "https://analytics-platform.com/ingest",
  "headers": "{\"X-API-Key\": \"YOUR_ANALYTICS_API_KEY\"}",
  "data_type": "analytics_event",
  "send_on_start": true,
  "send_on_close": true
}
```

### Custom Backend Integration

Connect to your own backend service:

```json
{
  "url": "https://your-backend.com/agent-conversations",
  "headers": "{\"Authorization\": \"Bearer YOUR_INTERNAL_TOKEN\", \"X-Source\": \"ten-agent\"}",
  "data_type": "agent_conversation",
  "send_on_start": true,
  "send_on_close": true
}
```

### Message Collector Integration

Connect to forward all data from a message collector:

```json
{
  "url": "https://your-logging-service.com/collector",
  "headers": "{\"Authorization\": \"Bearer YOUR_TOKEN\"}",
  "direct_forward": true
}
```

## Final vs. Non-Final Text

The speech-to-text (STT) systems often produce both non-final and final text:

- **Non-Final Text**: Interim results that may change as more audio is processed
- **Final Text**: Confirmed results that won't change

When `send_final_only` is set to `true`, only the final text is sent to the webhook, reducing the number of requests and ensuring only confirmed text is processed.

## Troubleshooting

If you're not seeing any messages sent to your webhook:

1. Check the logs to see if any data is being received
2. Verify the `url` is correctly set and accessible
3. Try enabling `direct_forward: true` if you're using a message collector
4. Make sure any needed authentication headers are properly formatted
5. If using `send_final_only: true`, check if your data has the `is_final` property set

## Development

For development, you can use a service like [webhook.site](https://webhook.site) to test the extension and see the data being sent.
