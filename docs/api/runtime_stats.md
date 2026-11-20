# Runtime statistics

[简体中文](runtime_stats.cn.md)

Read `ten:runtime_stats` using the existing TenEnv property APIs:

```python
import json

stats_json, error = await ten_env.get_property_to_json("ten:runtime_stats")
if error is None:
    stats = json.loads(stats_json)
    metrics = stats["metrics"]
```

Synchronous extensions use the same property path without `await`. Nested paths
also work, for example `ten:runtime_stats.metrics`.

The property is read-only and is generated on each read. Applications can poll
it periodically. It is separate from configured properties and does not appear
in `get_property_to_json("")`.

## Metrics

Enable runtime metrics using the existing app configuration:

```json
{
  "ten": {
    "services": {
      "telemetry": {
        "enabled": true,
        "metrics": {
          "enabled": true
        }
      }
    }
  }
}
```

The `exporter` field is optional. Configure it when external metrics output is
also needed.

The `metrics` field uses the OpenTelemetry protobuf JSON representation:

```json
{
  "metrics": {
    "resourceMetrics": [
      {
        "resource": { "attributes": [] },
        "scopeMetrics": [
          {
            "scope": { "name": "ten-framework" },
            "metrics": []
          }
        ]
      }
    ]
  }
}
```

Actual metrics retain their names, units, descriptions, attributes, timestamps,
and SDK aggregation fields. Histograms include `count`, `sum`, `explicitBounds`,
and `bucketCounts`. Counters and Histograms use cumulative temporality in the
SDK reader. Gauges retain the latest assigned value for each label set. Repeated
reads do not consume measurements or reset the existing exporter. Interval
counts, sums, and buckets can be calculated by subtracting consecutive readings
with the same attributes and start timestamp.

`extension_lifecycle_duration` is a Gauge reporting six stages: `creation`,
`on_configure`, `on_init`, `on_start`, `on_stop`, and `on_deinit`. The `creation`
duration starts when the runtime invokes the addon's `on_create_instance` callback and ends
when the runtime receives its matching `on_create_instance_done`. It includes
asynchronous creation work. The duration is captured before handing the result
back to the extension thread, so that handoff's queue time is excluded. All
durations use microseconds. Completed creation attempts that return a null
instance also report `creation`, using the requested extension name. Failures
before the addon creation callback is invoked do not report that stage.

`graph_startup_duration` is a Gauge in microseconds, labeled with `app_uri`
and `graph_id`. It starts when the receiving runtime's app receives the decoded
`start_graph` command, including any wait in the app's input queue, graph
parsing, routing, addon loading, and extension creation. It ends once all local
extension threads have created their extensions and are ready for lifecycle
execution. It does not wait for `on_configure`, `on_init`, or `on_start` to
complete. Earlier groups may already be executing those callbacks while other
groups are still creating extensions.

Each successfully created graph assigns its startup duration once. That value
remains fixed for the graph's ID across subsequent reads and requests targeting
the existing graph. Failures before creation readiness do not create a value.
Its SDK JSON value is available at `gauge.dataPoints[].asDouble`.
For an automatically started predefined graph, the receiving engine's internal
start request is the starting point. A graph spanning multiple apps has a
separate measurement in each receiving app; timestamps are local and are not
transmitted between apps.

All SDK scopes are returned, without filtering by graph or extension. Console,
Prometheus, and OTLP export continue to operate through their existing readers.

Extension creation and graph startup timing are collected by the C runtime
regardless of Rust API support. Exporting them through the SDK remains optional.

If the service hub is unavailable, including builds without Rust telemetry
support, the property returns:

```json
{ "metrics": { "resourceMetrics": [] } }
```

An enabled SDK reader with no measurements may return a resource entry with no
scope metrics. Reading after TenEnv closes, or when SDK collection fails,
returns an error through the existing property API.

## Inspect a real response

The runtime statistics smoke test prints the formatted JSON returned by the
extension's `get_property_to_json("ten:runtime_stats")` call:

```sh
./out/linux/x64/tests/standalone/ten_runtime_smoke_test \
  --gtest_filter='RuntimeStatsTest.ReadsSdkMetricsThroughProperties'
```

Look for `get_property_to_json("ten:runtime_stats") returned:` in the output.
To run all runtime statistics tests, use `--gtest_filter='RuntimeStatsTest.*'`.
Quote the filter so shells such as zsh pass the wildcard to Google Test.
