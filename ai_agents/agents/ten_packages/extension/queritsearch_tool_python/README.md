# queritsearch_tool_python

## Features

This extension provides a powerful search tool, backed by a trillion-scale multilingual index and purpose-built for LLM integration. It delivers real-time, high-precision search and retrieval, making it ideal for applications requiring fast and accurate information retrieval from a vast dataset.

## API

### Request Headers

| Header Name     | Type   | Description                                         |
|-----------------|--------|-----------------------------------------------------|
| Content-Type    | string | Required: `application/json`                        |
| Authorization   | string | Required: Bearer authentication parameter. For example: `Bearer {Your API KEY}`. You can obtain your API KEY from the API Key Management page. |

### Request Body Parameters

| Parameter       | Type    | Description                                                                                              |
|-----------------|---------|----------------------------------------------------------------------------------------------------------|
| query           | string  | Required: Your search query term.                                                                          |
| count           | integer | Optional: The maximum number of search results returned in the response.                                    |
| filters         | object  | Optional: Filter conditions, used to further refine the search results. Example structure for filters:     |
|                 |         | ```json                                                                                                   |
|                 |         | {                                                                                                        |
|                 |         |   "sites": { "include": [], "exclude": [] },                                                              |
|                 |         |   "timeRange": { "date": "" },                                                                            |
|                 |         |   "geo": { "countries": { "include": [] } },                                                              |
|                 |         |   "languages": { "include": [] }                                                                          |
|                 |         | }                                                                                                        |
|                 |         | ```                                                                                                      |

### Example Request

```bash
curl -s --compressed "https://api.querit.ai/v1/search" --header "Accept: application/json" --header "Authorization: Bearer 10000000" --data '{
  "query": "hello world",
  "count": 10
}'
```

### Example Response

```json
{
  "results": [
    { "title": "Hello World Introduction", "url": "https://example.com/hello-world" },
    { "title": "Hello World Tutorial", "url": "https://example.com/tutorial" },
    ...
  ]
}
```

## Development

### Build

To build the extension, ensure that you have the required dependencies set up:

- Install dependencies using `pip install -r requirements.txt`.
- Ensure Python 3.x is being used for development.

### Unit Test

To run unit tests for the extension, use the following command:

```bash
pytest tests/
```

Ensure that all tests pass successfully before deploying the extension to production.

## Misc

For additional details, refer to the [official API documentation](https://www.querit.ai/en/docs/).