# TEN Framework - Grafana Performance Monitoring

This directory contains a complete monitoring solution for TEN Framework applications using Prometheus and Grafana.

## 📊 Monitored Metrics

### 1. Extension Lifecycle Duration
Monitors the execution time of each Extension lifecycle stage:
- `on_configure` - Configuration stage
- `on_init` - Initialization stage
- `on_start` - Startup stage
- `on_stop` - Shutdown stage (only visible during app runtime)
- `on_deinit` - Cleanup stage (only visible during app runtime)

**Use Case**: Identify which Extensions have slow initialization or cleanup processes.

**Note**: `on_stop` and `on_deinit` metrics are only visible while the app is running. When the app shuts down, these metrics cannot be scraped by Prometheus (Pull mode limitation).

### 2. Extension CMD Processing Duration
Monitors the time each Extension takes to process different CMD messages:
- Tracked by `extension_name` and `msg_name` dimensions
- Shows P50 and P95 percentiles
- Provides average duration ranking table

**Use Case**: Identify which Extension + CMD combinations have poor performance.

### 3. Extension Thread Message Queue Wait Time
Monitors how long messages wait in Extension Thread queues:
- Tracked by `extension_group` dimension
- Shows P50 and P95 percentiles
- Provides average wait time ranking

**Use Case**: Identify which Extension Threads are overloaded, consider optimizing thread model or increasing concurrency.

## 🚀 Quick Start

### Prerequisites
- Docker
- Docker Compose

### For TEN Applications

1. **Copy this directory to your application root**:
   ```bash
   cp -r /home/sunxilin/ten-framework/tools/grafana-monitoring /path/to/your/ten-app/
   cd /path/to/your/ten-app/grafana-monitoring
   ```

2. **Update prometheus.yml** to point to your app's metrics endpoint:
   ```yaml
   scrape_configs:
     - job_name: 'ten-framework'
       static_configs:
         - targets: ['host.docker.internal:YOUR_METRICS_PORT']
   ```
   Replace `YOUR_METRICS_PORT` with your app's telemetry port (default: 49484).

3. **Ensure your app has telemetry enabled** in `property.json`:
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

4. **Start monitoring services**:
   ```bash
   docker-compose up -d
   ```

5. **Access Grafana**:
   - URL: http://localhost:3001
   - Username: `admin`
   - Password: `admin`

The dashboard "TEN Framework - Performance Monitoring" will be automatically loaded.

## 📈 Dashboard Panels

### Panel 1: Extension Lifecycle Duration
- **Type**: Time series (bar chart)
- **Metric**: `extension_lifecycle_duration`
- **Dimensions**: By Extension and lifecycle stage
- **Unit**: Microseconds (µs)

Shows the duration of all Extension lifecycle stages for easy comparison.

### Panel 2: Extension CMD Processing Duration (P50/P95)
- **Type**: Time series (line chart)
- **Metric**: `extension_cmd_processing_duration`
- **Dimensions**: By Extension name and CMD name
- **Unit**: Microseconds (µs)
- **Statistics**: P50 (median) and P95 (95th percentile)

Shows CMD processing performance trends. P95 reflects tail latency.

### Panel 3: Extension CMD Average Duration Ranking
- **Type**: Table
- **Metric**: Average of `extension_cmd_processing_duration`
- **Sorting**: By average duration descending

Quickly identifies which Extension + CMD combinations have the longest average duration.

### Panel 4: Extension Thread Message Queue Wait Time (P50/P95)
- **Type**: Time series (line chart)
- **Metric**: `extension_thread_msg_queue_stay_time`
- **Dimensions**: By Extension Group
- **Unit**: Microseconds (µs)
- **Statistics**: P50 and P95

Monitors message wait time in queues. Long wait times indicate thread overload.

### Panel 5: Extension Thread Message Queue Average Wait Time
- **Type**: Table
- **Metric**: Average of `extension_thread_msg_queue_stay_time`
- **Sorting**: By average wait time descending

Quickly identifies which Extension Threads have the heaviest load.

## 🛠️ Manual Operations

### Start containers only (without app)
```bash
docker-compose up -d
```

### View Prometheus metrics
```bash
curl http://localhost:49484/metrics
```

### Stop all services
```bash
docker-compose down
```

### Stop and clean up data
```bash
docker-compose down -v
```

## 🔍 Metric Details

### extension_lifecycle_duration
- **Type**: Gauge
- **Labels**:
  - `app_uri`: Application URI
  - `graph_id`: Graph ID
  - `extension_group`: Extension Group name
  - `extension`: Extension name
  - `stage`: Lifecycle stage (on_configure/on_init/on_start/on_stop/on_deinit)
- **Unit**: Microseconds

### extension_cmd_processing_duration
- **Type**: Histogram
- **Labels**:
  - `app_uri`: Application URI
  - `graph_id`: Graph ID
  - `extension_name`: Extension name
  - `msg_name`: CMD message name
- **Unit**: Microseconds
- **Description**: Time from when `on_cmd` is called to when the final `cmd_result` is returned

### extension_thread_msg_queue_stay_time
- **Type**: Histogram
- **Labels**:
  - `app_uri`: Application URI
  - `graph_id`: Graph ID
  - `extension_group`: Extension Group name
- **Unit**: Microseconds
- **Description**: Time from when a message enters the queue to when it starts being processed

## 📊 Performance Thresholds

### Lifecycle Duration
- Good: < 1 second (1,000,000 µs)
- Warning: If initialization takes > 1 second, consider optimization

### CMD Processing Duration
- Excellent: < 100ms (100,000 µs)
- Good: 100ms - 500ms
- Needs optimization: > 500ms (500,000 µs)

### Queue Wait Time
- Excellent: < 50ms (50,000 µs)
- Good: 50ms - 200ms
- Overloaded: > 200ms (200,000 µs)

## 🔧 Customization

### Modify Prometheus Configuration
Edit `prometheus.yml` to change scrape interval or targets:

```yaml
scrape_configs:
  - job_name: 'ten-framework'
    static_configs:
      - targets: ['localhost:49484']  # Change port
    scrape_interval: 5s  # Change interval
```

### Modify Grafana Dashboard
1. Login to Grafana
2. Enter dashboard edit mode
3. Modify queries or panel configurations
4. Save

### Export Dashboard Configuration
The dashboard JSON configuration is located at:
```
grafana/provisioning/dashboards/ten-framework-dashboard.json
```

## 📝 Troubleshooting

### Prometheus Cannot Scrape Metrics
1. Check if app is running: `curl http://localhost:49484/metrics`
2. View Prometheus logs: `docker logs ten_prometheus`
3. Check firewall settings

### Grafana Has No Data
1. Check Prometheus data source configuration (Grafana UI -> Configuration -> Data Sources)
2. Check if Prometheus has data: visit http://localhost:9091
3. Test query in Prometheus UI: `extension_lifecycle_duration`

### Dashboard Not Auto-loaded
1. Check Grafana logs: `docker logs ten_grafana`
2. Manually import dashboard: Settings -> Data Sources -> Import -> Upload JSON file

## 🌐 Port Configuration

Default ports:
- **Grafana**: 3001
- **Prometheus**: 9091
- **App Metrics**: 49484

To change ports, modify:
1. `docker-compose.yml` - Grafana and Prometheus ports
2. `prometheus.yml` - Metrics endpoint target
3. Your app's `property.json` - Metrics exporter port

## 📚 References

- [OpenTelemetry Documentation](https://opentelemetry.io/docs/)
- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)

## 🤝 Contributing

To improve the dashboard or add new metrics:
- Modify `grafana/provisioning/dashboards/ten-framework-dashboard.json` - Dashboard configuration
- Modify `prometheus.yml` - Prometheus configuration

## 📄 Files

```
grafana-monitoring/
├── docker-compose.yml           # Docker container configuration
├── prometheus.yml               # Prometheus scrape configuration
├── README.md                    # This file
└── grafana/
    └── provisioning/
        ├── datasources/
        │   └── prometheus.yml   # Grafana data source configuration
        └── dashboards/
            ├── dashboard.yml    # Dashboard auto-load configuration
            └── ten-framework-dashboard.json  # Dashboard definition
```
