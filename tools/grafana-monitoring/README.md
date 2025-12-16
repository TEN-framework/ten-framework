# TEN Framework - Grafana Performance Monitoring

This directory provides a complete monitoring solution for TEN Framework applications using Prometheus and Grafana.

## 🎯 Deployment Modes

We provide **three deployment configurations** to suit different needs:

| Mode | Use Case | Deploy Command | Config Files |
|------|----------|----------------|--------------|
| **Pull Mode** | Development, Simple Deployment | `docker-compose -f docker-compose.pull.yml up -d` | `configs/pull/` |
| **Push Mode** | Production, Cloud Native | `docker-compose -f docker-compose.push.yml up -d` | `configs/push/` |
| **Hybrid Mode** | Testing Both Modes | `docker-compose -f docker-compose.hybrid.yml up -d` | `configs/hybrid/` |

---

## 📊 Mode Comparison

### 1. Pull Mode (Prometheus Exporter)

**Architecture:** Application exposes metrics endpoint → Prometheus scrapes periodically

**Pros:**

- ✅ Simplest setup (only 2 components)
- ✅ No middleware needed
- ✅ Easy to debug

**Cons:**

- ❌ Requires port exposure
- ❌ Cannot capture shutdown metrics (on_stop, on_deinit)

**Best For:** Development, testing, long-running services

---

### 2. Push Mode (OTLP Exporter) ⭐ Recommended for Production

**Architecture:** Application pushes metrics → OTEL Collector → Prometheus

**Pros:**

- ✅ No port exposure needed (more secure)
- ✅ Captures full lifecycle metrics (on_stop, on_deinit)
- ✅ Cloud-native architecture
- ✅ Supports complex data routing and multi-backend export

**Cons:**

- ❌ Requires OTEL Collector deployment
- ❌ More complex configuration

**Best For:** Production, cloud-native deployments, Kubernetes, short-lived processes

---

### 3. Hybrid Mode

**Architecture:** Supports both Pull and Push modes simultaneously

**Pros:**

- ✅ Compare both modes side-by-side
- ✅ Smooth migration path
- ✅ Different apps can use different modes

**Cons:**

- ❌ Higher resource consumption

**Best For:** A/B testing, gradual migration between modes

---

## 📋 Quick Comparison Table

| Feature | Pull Mode | Push Mode | Hybrid Mode |
|---------|-----------|-----------|-------------|
| **Setup Complexity** | ⭐ Simple | ⭐⭐ Medium | ⭐⭐ Medium |
| **Components** | 2 | 3 | 3 |
| **Port Exposure** | Required | Not Required | Optional |
| **Full Lifecycle** | ❌ | ✅ | Configurable |
| **Cloud Native** | Basic | ✅ | ✅ |
| **Best For** | Dev/Test | Production | Testing/Migration |

---

## 🚀 Quick Start

### Pull Mode (Simplest)

```bash
# 1. Start monitoring stack
cd /path/to/grafana-monitoring
docker-compose -f docker-compose.pull.yml up -d

# 2. Configure your TEN application (property.json)
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
              "endpoint": "0.0.0.0:49484",
              "path": "/metrics"
            }
          }
        }
      }
    }
  }
}

# 3. Start your TEN application
./your_ten_application

# 4. Access Grafana
# URL: http://localhost:3001
# Username: admin
# Password: admin
```

---

### Push Mode (Production)

```bash
# 1. Start monitoring stack
cd /path/to/grafana-monitoring
docker-compose -f docker-compose.push.yml up -d

# 2. Configure your TEN application (property.json)
{
  "ten": {
    "services": {
      "telemetry": {
        "enabled": true,
        "metrics": {
          "enabled": true,
          "exporter": {
            "type": "otlp",
            "config": {
              "endpoint": "http://localhost:4317",
              "protocol": "grpc"
            }
          }
        }
      }
    }
  }
}

# 3. Start your TEN application
./your_ten_application

# 4. Verify
docker logs -f ten_otel_collector_push
curl http://localhost:8889/metrics | grep ten_

# 5. Access Grafana
# URL: http://localhost:3001
# Username: admin
# Password: admin
```

---

### Hybrid Mode (Testing)

```bash
# 1. Start monitoring stack
cd /path/to/grafana-monitoring
docker-compose -f docker-compose.hybrid.yml up -d

# 2. Run multiple apps with different exporters
# - App 1: Use Prometheus exporter (type: "prometheus")
# - App 2: Use OTLP exporter (type: "otlp")

# 3. Compare in Grafana using mode label
# Pull mode: {mode="pull"}
# Push mode: {mode="push"}
```

---

## 📊 Monitored Metrics

### 1. Extension Lifecycle Duration

Monitors the execution time of each Extension lifecycle stage:

- `on_configure` - Configuration stage
- `on_init` - Initialization stage
- `on_start` - Startup stage
- `on_stop` - Shutdown stage
- `on_deinit` - Cleanup stage

**Labels:**

- `app_uri`: Application URI
- `graph_id`: Graph ID
- `extension_group`: Extension Group name
- `extension`: Extension name
- `stage`: Lifecycle stage

**Use Case:** Identify slow initialization or cleanup processes

---

### 2. Extension CMD Processing Duration

Monitors command processing time with histogram buckets:

- P50 and P95 percentiles
- Average duration ranking

**Labels:**

- `app_uri`: Application URI
- `graph_id`: Graph ID
- `extension_name`: Extension name
- `msg_name`: CMD message name

**Use Case:** Identify slow command handlers

---

### 3. Extension Thread Message Queue Wait Time

Monitors message queue wait time with histogram buckets:

- P50 and P95 percentiles
- Average wait time by extension group

**Labels:**

- `app_uri`: Application URI
- `graph_id`: Graph ID
- `extension_group`: Extension Group name

**Use Case:** Identify overloaded extension threads

---

## 📈 Grafana Dashboard

The dashboard "TEN Framework - Performance Monitoring" is automatically loaded and includes:

### Panel 1: Extension Lifecycle Duration

- **Type:** Time series (bar chart)
- **Unit:** Microseconds (µs)
- Shows all lifecycle stages for comparison

### Panel 2: Extension CMD Processing Duration (P50/P95)

- **Type:** Time series (line chart)
- **Unit:** Microseconds (µs)
- Shows median and 95th percentile latency

### Panel 3: Extension CMD Average Duration Ranking

- **Type:** Table
- Sorted by average duration (descending)
- Quickly identifies slowest command handlers

### Panel 4: Extension Thread Message Queue Wait Time (P50/P95)

- **Type:** Time series (line chart)
- **Unit:** Microseconds (µs)
- Monitors queue congestion

### Panel 5: Extension Thread Message Queue Average Wait Time

- **Type:** Table
- Sorted by average wait time (descending)
- Identifies thread bottlenecks

---

## 📊 Performance Thresholds

### Lifecycle Duration

- **Good:** < 1 second (1,000,000 µs)
- **Warning:** > 1 second (consider optimization)

### CMD Processing Duration

- **Excellent:** < 100ms (100,000 µs)
- **Good:** 100ms - 500ms
- **Needs Optimization:** > 500ms (500,000 µs)

### Queue Wait Time

- **Excellent:** < 50ms (50,000 µs)
- **Good:** 50ms - 200ms
- **Overloaded:** > 200ms (200,000 µs)

---

## 🔧 Configuration

### Application Configuration

Configure your TEN application's `property.json` file with the appropriate exporter type:

**Pull Mode (Prometheus Exporter):**

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
              "endpoint": "0.0.0.0:49484",
              "path": "/metrics"
            }
          }
        }
      }
    }
  }
}
```

**Push Mode (OTLP Exporter):**

```json
{
  "ten": {
    "services": {
      "telemetry": {
        "enabled": true,
        "metrics": {
          "enabled": true,
          "exporter": {
            "type": "otlp",
            "config": {
              "endpoint": "http://localhost:4317",
              "protocol": "grpc"
            }
          }
        }
      }
    }
  }
}
```

### Prometheus Configuration

Each mode has its own Prometheus configuration:

- Pull Mode: `configs/pull/prometheus.yml`
- Push Mode: `configs/push/prometheus.yml`
- Hybrid Mode: `configs/hybrid/prometheus.yml`

### OTEL Collector Configuration

For Push and Hybrid modes:

- Push Mode: `configs/push/otel-collector-config.yml`
- Hybrid Mode: `configs/hybrid/otel-collector-config.yml`

---

## 🛠️ Common Operations

### View Service Status

```bash
# Pull mode
docker-compose -f docker-compose.pull.yml ps

# Push mode
docker-compose -f docker-compose.push.yml ps

# Hybrid mode
docker-compose -f docker-compose.hybrid.yml ps
```

### View Logs

```bash
# Prometheus
docker logs ten_prometheus_[pull|push|hybrid]

# Grafana
docker logs ten_grafana_[pull|push|hybrid]

# OTEL Collector (Push/Hybrid only)
docker logs ten_otel_collector_[push|hybrid]
```

### Stop Services

```bash
# Stop and remove containers
docker-compose -f docker-compose.pull.yml down

# Stop and remove containers + volumes (clean data)
docker-compose -f docker-compose.pull.yml down -v
```

### Restart Services

```bash
docker-compose -f docker-compose.pull.yml restart
```

---

## 🌐 Port Configuration

### Pull Mode Ports

- **49484:** Application metrics endpoint
- **9091:** Prometheus UI
- **3001:** Grafana UI

### Push Mode Ports

- **4317:** OTLP gRPC receiver
- **4318:** OTLP HTTP receiver
- **8889:** OTEL Collector Prometheus exporter
- **9091:** Prometheus UI
- **3001:** Grafana UI

### Hybrid Mode Ports

- **All of the above**

---

## 📝 Troubleshooting

### Prometheus Cannot Scrape Metrics (Pull Mode)

1. Check if application is running and exposing metrics:

   ```bash
   curl http://localhost:49484/metrics
   ```

2. Check Prometheus logs:

   ```bash
   docker logs ten_prometheus_pull
   ```

3. Verify Prometheus targets status:
   - Visit: <http://localhost:9091/targets>

4. Check network connectivity (especially on Linux):
   - Replace `host.docker.internal` with `172.17.0.1` or your host IP in `prometheus.yml`

---

### OTEL Collector Not Receiving Data (Push Mode)

1. Check if Collector is running:

   ```bash
   docker ps | grep otel_collector
   ```

2. Check Collector logs:

   ```bash
   docker logs ten_otel_collector_push
   ```

3. Verify port accessibility:

   ```bash
   telnet localhost 4317
   ```

4. Check application configuration:
   - Ensure `endpoint` is correct: `http://localhost:4317`
   - Verify `protocol` is set to `grpc` or `http`

---

### Grafana Shows No Data

1. Check if Prometheus has data:
   - Visit: <http://localhost:9091>
   - Query: `extension_lifecycle_duration` (Pull) or `ten_extension_lifecycle_duration` (Push)

2. Verify Grafana data source:
   - Grafana UI → Configuration → Data Sources → Prometheus
   - Click "Test" button

3. Check dashboard time range:
   - Set to "Last 5 minutes" or "Last 15 minutes"

4. Ensure application is running and generating metrics

---

## 🔄 Switching Between Modes

### From Pull to Push

```bash
# Stop Pull mode
docker-compose -f docker-compose.pull.yml down

# Start Push mode
docker-compose -f docker-compose.push.yml up -d

# Update application configuration to use OTLP exporter
# Restart application
```

### From Push to Pull

```bash
# Stop Push mode
docker-compose -f docker-compose.push.yml down

# Start Pull mode
docker-compose -f docker-compose.pull.yml up -d

# Update application configuration to use Prometheus exporter
# Restart application
```

---

## 📁 Directory Structure

```text
grafana-monitoring/
├── README.md                          # This file
├── docker-compose.pull.yml            # Pull mode deployment
├── docker-compose.push.yml            # Push mode deployment
├── docker-compose.hybrid.yml          # Hybrid mode deployment
├── configs/
│   ├── pull/
│   │   └── prometheus.yml             # Pull mode Prometheus config
│   ├── push/
│   │   ├── otel-collector-config.yml  # Push mode Collector config
│   │   └── prometheus.yml             # Push mode Prometheus config
│   └── hybrid/
│       ├── otel-collector-config.yml  # Hybrid mode Collector config
│       └── prometheus.yml             # Hybrid mode Prometheus config
└── grafana/
    └── provisioning/
        ├── datasources/
        │   └── prometheus.yml         # Grafana data source config
        └── dashboards/
            ├── dashboard.yml          # Dashboard auto-load config
            └── ten-framework-dashboard.json  # Dashboard definition
```

---

## 💡 Recommendations

- **Development Environment:** Use Pull Mode or Hybrid Mode
- **Testing Environment:** Use the same mode you plan to use in production
- **Production Environment:** Use Push Mode (recommended) or Pull Mode (for simple cases)
- **Migration Period:** Use Hybrid Mode for gradual transition
- **Not Sure?** Start with Pull Mode, you can switch anytime

---

## 📚 References

- [OpenTelemetry Documentation](https://opentelemetry.io/docs/)
- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)
- [TEN Framework Documentation](https://doc.theten.ai)

---

## 🤝 Contributing

To improve the monitoring setup:

- Modify dashboard: `grafana/provisioning/dashboards/ten-framework-dashboard.json`
- Modify Prometheus config: `configs/[mode]/prometheus.yml`
- Modify Collector config: `configs/[mode]/otel-collector-config.yml`

---

**Need Help?** Check the Troubleshooting section or review the configuration examples in the `configs/` directory.
