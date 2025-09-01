# oceanbase_powerrag_python

OceanBase PowerRAG Python extension for TEN Framework. It connects to the PowerRAG Chat API and streams text replies.

## Features

- Streamed chat responses (SSE data lines)
- Outputs text only (answer.content)
- Configurable via `property.json` or environment variables
- Optional greeting and failure fallback

## Configuration

Properties (see [manifest.json] and [property.json](property.json)):

- `base_url` (required): API base
  - Example: `http://mi.aliyun-ap-1-internet.oceanbase.cloud:8081/oceanbase/ai/databases`
- `api_key` (required): OceanBase PowerRAG API key
- `ai_database_name` (required)
- `collection_id` (required)
- `user_id` (optional, default: `User`)
- `greeting` (optional)
- `failure_info` (optional)

Environment variables used by default:

```bash
export OCEANBASE_BASE_URL="<your_base_url>"
export OCEANBASE_API_KEY="<your_api_key>"
export OCEANBASE_AI_DATABASE_NAME="<your_db>"
export OCEANBASE_COLLECTION_ID="<your_collection>"
```

## API

- Endpoint (PUT):
  - `{base_url}/{ai_database_name}/collections/{collection_id}/chat`
- Request body:

```json
{
  "stream": true,
  "jsonFormat": true,
  "content": "your question"
}
```

- Response: SSE lines with `data:{...}`. This extension only forwards `answer.content` as text.

## Development

### Build

Python-only; no compile step required. If needed, install dependencies via your environment manager. In TEN Agents container, you can run:

```bash
cd ai_agents/agents/ten_packages/extension/oceanbase_powerrag_python
tman -y install --standalone
```

### Test (quick)

- Use TEN Playground to enable this extension in a graph, set properties, start the graph, then send `text_data` with `is_final=true` and a `text` string.

## Troubleshooting

- Non-200 response: check `api_key`, `base_url` format, `ai_database_name`, `collection_id`
- No output: verify you sent final text (`is_final=true`), and logs in TEN show streaming lines
- Set `failure_info` to return a friendly error fallback on connection failure
