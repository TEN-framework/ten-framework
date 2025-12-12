# Quick Start Example

This is a quick example showing how to use the Grafana monitoring with a TEN Framework application.

## Example: Monitoring the long_running Test App

The `long_running` test app already has the monitoring configured. Here's how it was set up:

### 1. Property Configuration

In `/path/to/long_running_app/property.json`:

```json
{
  "ten": {
    "services": {
      "telemetry": {
        "enabled": true,
        "metrics": {
          "enabled": true,
          "exporter": {
            "type": "prometheus",
            "config": {
              "host": "0.0.0.0",
              "port": 49484,
              "path": "/metrics"
            }
          }
        }
      }
    }
  }
}
```

### 2. Prometheus Configuration

In `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'ten-framework'
    static_configs:
      - targets: ['host.docker.internal:49484']  # Points to app metrics endpoint
    scrape_interval: 5s
```

### 3. Start Everything

```bash
# Start your TEN app
cd /path/to/your/app
./bin/your_app &

# Start monitoring
cd /path/to/grafana-monitoring
docker-compose up -d

# Wait a few seconds for services to start
sleep 10

# Access Grafana
# Open browser to http://localhost:3001
# Login with admin/admin
```

### 4. View Metrics

You should see 5 panels in the dashboard:
1. Extension Lifecycle Duration (bar chart)
2. Extension CMD Processing Duration P50/P95 (line chart)
3. Extension CMD Average Duration Ranking (table)
4. Extension Thread Message Queue Wait Time P50/P95 (line chart)
5. Extension Thread Message Queue Average Wait Time (table)

### 5. Verify Data

Check if metrics are being collected:

```bash
# Check app metrics endpoint
curl http://localhost:49484/metrics | grep extension

# Check Prometheus has data
curl "http://localhost:9091/api/v1/query?query=extension_lifecycle_duration"
```

### 6. Stop Everything

```bash
# Stop monitoring
cd /path/to/grafana-monitoring
docker-compose down

# Stop your app
pkill -f your_app
```

## Adapting for Your Application

### Step 1: Enable Telemetry

Add to your app's `property.json`:

```json
{
  "ten": {
    "services": {
      "telemetry": {
        "enabled": true,
        "metrics": {
          "enabled": true,
          "exporter": {
            "type": "prometheus",
            "config": {
              "host": "0.0.0.0",
              "port": 49484,
              "path": "/metrics"
            }
          }
        }
      }
    }
  }
}
```

### Step 2: Copy Monitoring Config

```bash
cp -r /home/sunxilin/ten-framework/tools/grafana-monitoring /path/to/your/app/
```

### Step 3: Update prometheus.yml

Change the port in `prometheus.yml` if your app uses a different port:

```yaml
- targets: ['host.docker.internal:YOUR_PORT']  # Change 49484 to your port
```

### Step 4: Run

```bash
cd /path/to/your/app
./bin/your_app &

cd grafana-monitoring
docker-compose up -d
```

## Common Issues

### Port Conflicts

If ports 3001 or 9091 are already in use, edit `docker-compose.yml`:

```yaml
grafana:
  ports:
    - "YOUR_GRAFANA_PORT:3000"  # Change 3001 to any free port

prometheus:
  ports:
    - "YOUR_PROMETHEUS_PORT:9090"  # Change 9091 to any free port
```

### No Data in Grafana

1. Verify app is running: `ps aux | grep your_app`
2. Check metrics endpoint: `curl http://localhost:49484/metrics`
3. Check Prometheus targets: http://localhost:9091/targets
4. Ensure the target status is "UP"

### Dashboard Shows Wrong Data Source

If you see "DataSource prometheus was not found":

1. Login to Grafana (http://localhost:3001)
2. Go to Configuration -> Data Sources
3. Note the UID of the Prometheus data source
4. Edit `grafana/provisioning/dashboards/ten-framework-dashboard.json`
5. Replace all occurrences of the UID with the correct one
6. Restart Grafana: `docker-compose restart grafana`
