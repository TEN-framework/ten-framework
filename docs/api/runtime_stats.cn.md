# 运行时统计

[English](runtime_stats.md)

通过现有的 TenEnv 属性 API 读取 `ten:runtime_stats`：

```python
import json

stats_json, error = await ten_env.get_property_to_json("ten:runtime_stats")
if error is None:
    stats = json.loads(stats_json)
    metrics = stats["metrics"]
```

同步 extension 使用相同的属性路径，调用时去掉 `await`。也支持读取嵌套路径，
例如 `ten:runtime_stats.metrics`。

该属性只读，每次读取时动态生成结果。业务可以定期轮询。它与配置属性独立，
不会出现在 `get_property_to_json("")` 返回的配置中。

## 指标

通过现有的 app 配置开启 runtime 指标：

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

`exporter` 字段可选。需要将指标输出到外部时，再配置对应的 exporter。

对于 OTLP metrics exporter，`config.endpoint` 可选。例如，将
`ten.services.telemetry` 配置为：

```json
{
  "enabled": true,
  "metrics": {
    "enabled": true,
    "exporter": { "type": "otlp" }
  }
}
```

runtime 按以下优先级读取 endpoint：

1. 非空白的 `exporter.config.endpoint`。
2. `OTEL_EXPORTER_OTLP_METRICS_ENDPOINT`。

配置中的 endpoint 缺省、为空字符串或仅包含空白时，使用环境变量。
如果没有可用的 endpoint，或者选中的值不合法，则关闭外部导出。
高优先级的值不合法时，不会继续尝试其他地址。
runtime 不读取 `OTEL_EXPORTER_OTLP_ENDPOINT`，也不会使用 SDK 默认的 localhost collector。
OTLP 初始化失败时同样关闭外部导出，应用继续运行。
这些情况下，SDK 仍然聚合指标，可以通过 `ten:runtime_stats.metrics` 读取。

协议默认为 `grpc`。当 `config.protocol` 为 `http` 时，metrics 专用环境变量和显式配置
都需要提供完整的 endpoint URL，包括 `/v1/metrics` 等路径；runtime 不追加路径。
endpoint 必须使用 `http://` 或 `https://`，主机和端口合法，且不包含空白或 URL fragment。
初始化期间不会检查 collector 是否可达。

`metrics` 字段使用 OpenTelemetry protobuf 的 JSON 表示，结构如下：

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

实际返回的数据保留指标名称、单位、描述、属性、时间戳和 SDK 聚合字段。
Histogram 包含 `count`、`sum`、`explicitBounds` 和 `bucketCounts`。
SDK reader 对 Counter 和 Histogram 使用累计聚合，Gauge 则为每组标签保留最近一次
设定的值。重复读取不会消耗测量数据，也不会重置现有 exporter 的数据。
当属性和起始时间戳相同时，可以对连续两次读取的计数、总和及桶计数求差，得到区间统计。

`extension_lifecycle_duration` 是 Gauge，包含六个阶段：`creation`、
`on_configure`、`on_init`、`on_start`、`on_stop` 和 `on_deinit`。
`creation` 从 runtime 调用 addon 的 `on_create_instance` 回调开始，
到 runtime 收到对应的 `on_create_instance_done` 为止，包含异步创建期间的耗时。
runtime 在将结果交回 extension 线程之前收口计时，因此这次线程交接的排队时间
不会计入 `creation`。所有耗时均以微秒为单位。
如果创建回调执行后通过空实例表示创建失败，也会使用请求中的 extension 名称记录
`creation`。在进入 addon 创建回调之前发生的失败不会记录该阶段。

`graph_startup_duration` 是 Gauge，单位为微秒，标签为 `app_uri` 和 `graph_id`。
计时从接收方 runtime 的 app 收到解码后的 `start_graph` 命令开始，
包含 app 输入队列等待、图解析、路由、addon 加载和 extension 创建的耗时。
当本地所有 extension 线程都已创建好各自的 extension，并具备执行生命周期的条件时，
结束计时。它不会等待 `on_configure`、`on_init` 或 `on_start` 执行完成。
在部分 group 仍在创建 extension 时，较早就绪的 group 可能已经开始执行这些回调。

每个成功创建的 graph 只设置一次启动耗时。对于该 graph ID，后续读取及针对已有 graph
的请求都不会改变这个值。在创建就绪之前发生的失败不会产生该指标值。
SDK JSON 中的数值位于 `gauge.dataPoints[].asDouble`。
对于预定义的自动启动图，以接收方 engine 的内部启动请求作为计时起点。
当 graph 跨多个 app 时，各接收方 app 分别计时；时间戳保留在本地，不会在 app 之间传递。

返回结果包含 SDK 中的全部 scope，不按 graph 或 extension 过滤。
Console、Prometheus 和 OTLP 继续通过各自现有的 reader 输出指标。

extension 创建和 graph 启动的计时由 C runtime 采集，与是否支持 Rust API 无关。
是否通过 SDK 导出指标可以另行选择。

如果 service hub 不可用，包括未启用 Rust telemetry 支持的构建，该属性返回：

```json
{ "metrics": { "resourceMetrics": [] } }
```

如果 SDK reader 已启用但还没有测量数据，返回结果可能包含一个没有 scope 指标的
resource 条目。在 TenEnv 关闭后读取，或者 SDK 采集失败时，会通过现有属性 API 返回错误。

## 查看实际返回内容

运行时统计的 smoke test 会打印 extension 调用
`get_property_to_json("ten:runtime_stats")` 得到的格式化 JSON：

```sh
./out/linux/x64/tests/standalone/ten_runtime_smoke_test \
  --gtest_filter='RuntimeStatsTest.ReadsSdkMetricsThroughProperties'
```

在输出中查找 `get_property_to_json("ten:runtime_stats") returned:`。
要运行全部运行时统计测试，使用 `--gtest_filter='RuntimeStatsTest.*'`。
过滤参数需要加引号，避免 zsh 等 shell 展开通配符，让 Google Test 接收原始过滤条件。
