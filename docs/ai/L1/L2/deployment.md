# Deployment

> **When to Read This:** Load this document when you are deploying to production,
> setting up HTTPS access, configuring monitoring, or ensuring services persist
> across session closures.

## Docker Compose Setup

The development container is defined in `ai_agents/docker-compose.yml`:

```yaml
services:
  ten_agent_dev:
    image: ghcr.io/ten-framework/ten_agent_build:0.7.14
    container_name: ten_agent_dev
    ports:
      - "49483:49483"   # TMAN Designer
      - "3000:3000"     # Playground
      - "8000-9001:8000-9001"  # API + worker range
    volumes:
      - .:/app
    environment:
      - LOG_PATH=${LOG_PATH}
```

Start: `cd ai_agents && docker compose up -d`

## Persistent Startup (Survives Session Closure)

Use `-d` flag with `docker exec` to keep services running after terminal disconnect:

```bash
# 1. Clean up existing processes
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "ps -elf | grep 'bin/worker --property' | grep -v grep | awk '{print \$4}' | xargs -r kill -9 2>/dev/null"

# 2. Remove stale lock files
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"

# 3. Install Python dependencies
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/<example>/tenapp && bash scripts/install_python_deps.sh"

# 4. Start everything in detached mode
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/<example> && task run > /tmp/task_run.log 2>&1"

# 5. Wait and verify
sleep 15
curl -s http://localhost:8080/health && echo " API ready"
curl -s http://localhost:8080/graphs | jq -r '.data | length' | xargs echo "Graphs:"
curl -s http://localhost:3000 -o /dev/null -w '%{http_code}' | xargs echo "Playground:"
```

Key: `-d` flag keeps processes running. `task run` starts API + playground + TMAN Designer.

## Production Graph Picker Mode

The playground uses two graph APIs depending on frontend mode:

| Mode | Endpoint |
| ---- | -------- |
| Edit mode | `/api/dev/v1/graphs` |
| Normal mode | `/api/agents/graphs` |

If production is accidentally started with `NEXT_PUBLIC_EDIT_GRAPH_MODE=true`,
the graph picker may be empty and the browser may show `403 Forbidden` for
`/api/dev/v1/graphs` even though the backend is healthy.

For normal production demos, set:

```bash
NEXT_PUBLIC_EDIT_GRAPH_MODE=false
```

## Cloudflare Tunnel (Free HTTPS)

Quick HTTPS access without domain or SSL setup:

```bash
# Start tunnel
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5

# Get the random URL
grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
# Example: https://films-colon-msgid-incentives.trycloudflare.com
```

- Free tunnels get **random URLs** that change on restart
- No DNS configuration needed
- Good for development and demos

## Nginx Reverse Proxy (Production HTTPS)

For production with custom domain and SSL certificates:

```nginx
server {
    listen [::]:443 ssl ipv6only=on;
    listen 443 ssl;
    ssl_certificate /etc/letsencrypt/live/oai.agora.io/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/oai.agora.io/privkey.pem;
    include /etc/letsencrypt/options-ssl-nginx.conf;
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;

    # API endpoints
    location ~ ^/(health|ping|token|start|stop|graphs|list)(/|$) {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Playground (with WebSocket upgrade)
    location / {
        proxy_pass http://localhost:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

Apply: `sudo nginx -t && sudo systemctl reload nginx`

## Production Build

```bash
# Build optimized frontend
docker exec ten_agent_dev bash -c "cd /app/playground && npm run build"

# Start production server
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm start > /tmp/playground_prod.log 2>&1"
```

## Grafana Monitoring

Located in `tools/grafana-monitoring/`. Three deployment modes:

### Pull Mode (Development)

Prometheus scrapes a metrics endpoint exposed by the TEN runtime:

```json
// In property.json
{
  "ten": {
    "exporter": {
      "enabled": true,
      "type": "prometheus",
      "prometheus": {
        "listen_address": "0.0.0.0",
        "listen_port": 49484
      }
    }
  }
}
```

Setup: `cd tools/grafana-monitoring && docker compose -f docker-compose.pull.yml up -d`

### Push Mode (Production)

Uses OTEL Collector to push metrics to Prometheus and logs to Loki:

```json
// In property.json
{
  "ten": {
    "exporter": {
      "enabled": true,
      "type": "otlp",
      "otlp": {
        "endpoint": "http://otel-collector:4317"
      }
    }
  }
}
```

Setup: `cd tools/grafana-monitoring && docker compose -f docker-compose.push.yml up -d`

### Hybrid Mode

Both Pull and Push simultaneously — useful for A/B testing or migration.

### Monitored Metrics

| Metric                            | Good Threshold | What It Measures                    |
| --------------------------------- | -------------- | ----------------------------------- |
| Extension Lifecycle Duration      | < 1 second     | on_configure, on_init, on_start, on_stop, on_deinit |
| Extension CMD Processing Duration | < 100ms        | P50/P95 command handling time       |
| Thread Message Queue Wait Time    | < 50ms         | Time messages wait before processing|

### Log Aggregation (Push Mode Only)

Push mode sends logs to Loki for centralized querying:

```
# LogQL query examples
{service_name="ten_agent"} |= "error"
{service_name="ten_agent"} | json | level="error"
{service_name="ten_agent"} |= "deepgram" | json
```

## After Container Restart Checklist

1. Reinstall Python dependencies (not persisted)
2. Start server with `task run`
3. Restart Cloudflare tunnel (if using)
4. Kill any zombie worker processes on host
5. Verify with `/health` and `/graphs` endpoints

## Portal References

- [Deploy Agent Service](https://github.com/TEN-framework/portal/blob/main/content/docs/ten_agent_examples/deploy_ten_agent/deploy_agent_service.md) [EXTERNAL] — official deployment guide

## See Also

- [Back to Setup](../01_setup.md)
- [Server Architecture](server_architecture.md) — Worker lifecycle, session management
