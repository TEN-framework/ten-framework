---
title: 日志
_portal_target: api/log.cn.md
---

TEN framework 允许使用不同语言开发的 extension 在同一进程中运行。这使得需要以统一的日志格式和信息在统一的日志中查看所有这些 extension 的日志，从而简化调试。

为了解决这个问题，`ten_env` 提供了一个日志 API。在 extension 的每个回调中，都可以访问 `ten_env` 的实例。使用此实例，您可以调用日志 API，将使用各种语言开发的 extension 的日志输出到统一的日志输出中。

## 日志级别

TEN framework 支持以下日志级别，按严重程度递增：

- **DEBUG** (1): 调试信息
- **INFO** (2): 一般信息
- **WARN** (3): 警告信息
- **ERROR** (4): 错误信息

## API 接口

### C++ 语言

```cpp
#include "ten_runtime/binding/cpp/ten.h"

class Extension {
public:
    void on_cmd(ten::ten_env_t& ten_env, std::unique_ptr<ten::cmd_t> cmd) {
        // 使用便捷宏
        TEN_ENV_LOG_DEBUG(ten_env, "Processing command");
        TEN_ENV_LOG_INFO(ten_env, "Command name: " + cmd->get_name());
        TEN_ENV_LOG_WARN(ten_env, "Warning message");
        TEN_ENV_LOG_ERROR(ten_env, "Error occurred");

        // 直接调用方法，支持 category 和 fields 参数
        ten_env.log(TEN_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__,
                   "Direct log call", nullptr, nullptr);
    }
};
```

C++ 提供了以下便捷宏：

- `TEN_ENV_LOG_DEBUG(ten_env, msg)`
- `TEN_ENV_LOG_INFO(ten_env, msg)`
- `TEN_ENV_LOG_WARN(ten_env, msg)`
- `TEN_ENV_LOG_ERROR(ten_env, msg)`

### Python 语言

```python
from ten_runtime import Extension, TenEnv

class MyExtension(Extension):
    def on_cmd(self, ten_env: TenEnv, cmd):
        # 使用不同级别的日志方法
        ten_env.log_debug("调试信息")
        ten_env.log_info("一般信息")
        ten_env.log_warn("警告信息")
        ten_env.log_error("错误信息")

        # 支持格式化字符串和额外参数
        test_value = "example"
        ten_env.log_info(f"处理命令，测试值: {test_value}")

        # 支持 category 和 fields 参数
        ten_env.log_info("带分类的日志", category="my_extension")
```

Python 提供了以下方法：

- `log_debug(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_info(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_warn(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_error(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`

### Go 语言

```go
package main

import "ten_runtime/ten"

type MyExtension struct {
    ten.DefaultExtension
}

func (ext *MyExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
    // 使用不同级别的日志方法
    tenEnv.LogDebug("调试信息")
    tenEnv.LogInfo("一般信息")
    tenEnv.LogWarn("警告信息")
    tenEnv.LogError("错误信息")

    // 支持格式化
    cmdName := cmd.GetName()
    tenEnv.LogInfo("处理命令: " + cmdName)

    // 使用完整的 Log 方法支持更多参数
    category := "my_extension"
    tenEnv.Log(ten.LogLevelInfo, "带分类的日志", &category, nil, nil)
}
```

Go 提供了以下方法：

- `LogDebug(msg string) error`
- `LogInfo(msg string) error`
- `LogWarn(msg string) error`
- `LogError(msg string) error`
- `Log(level LogLevel, msg string, category *string, fields *Value, option *TenLogOption) error`

### Node.js/TypeScript 语言

```typescript
import { Extension, TenEnv, Cmd } from "ten-runtime-nodejs";

class MyExtension extends Extension {
    async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
        // 使用不同级别的日志方法
        tenEnv.logDebug("调试信息");
        tenEnv.logInfo("一般信息");
        tenEnv.logWarn("警告信息");
        tenEnv.logError("错误信息");

        // 支持字符串拼接
        const cmdName = cmd.getName();
        tenEnv.logInfo("处理命令: " + cmdName);

        // 支持 category 和 fields 参数
        tenEnv.logInfo("带分类的日志", "my_extension");
    }
}
```

Node.js 提供了以下方法：

- `logDebug(message: string, category?: string, fields?: Value): TenError | undefined`
- `logInfo(message: string, category?: string, fields?: Value): TenError | undefined`
- `logWarn(message: string, category?: string, fields?: Value): TenError | undefined`
- `logError(message: string, category?: string, fields?: Value): TenError | undefined`

## 使用示例

### 基本使用

```python
# Python 示例
def on_start(self, ten_env: TenEnv):
    ten_env.log_info("extension 启动")
    ten_env.on_start_done()

def on_cmd(self, ten_env: TenEnv, cmd):
    cmd_name = cmd.get_name()
    ten_env.log_debug(f"收到命令: {cmd_name}")

    # 处理命令逻辑
    if cmd_name == "hello":
        ten_env.log_info("处理 hello 命令", category="command_handler")
        # ... 处理逻辑
    else:
        ten_env.log_warn(f"未知命令: {cmd_name}")
```

### 在线程中使用

```python
import threading
import time

def log_routine(self, ten_env: TenEnv):
    """在单独线程中记录日志"""
    while not self.stop_flag:
        self.log_count += 1
        ten_env.log_info(f"日志消息 {self.log_count}")
        time.sleep(0.05)

def on_start(self, ten_env: TenEnv):
    # 启动日志线程
    self.log_thread = threading.Thread(
        target=self.log_routine, args=(ten_env,)
    )
    self.log_thread.start()
    ten_env.on_start_done()
```

### 错误处理

```python
def on_cmd(self, ten_env: TenEnv, cmd):
    try:
        # 业务逻辑
        result = self.process_command(cmd)
        ten_env.log_info("命令处理成功")
    except ValueError as e:
        ten_env.log_error(f"参数错误: {str(e)}", category="error_handling")
    except Exception as e:
        ten_env.log_error(f"严重错误: {str(e)}", category="error_handling")
        raise
```

## 日志格式和输出

TEN framework 的日志系统会自动包含以下信息：

- **时间戳**: 日志记录的时间
- **日志级别**: DEBUG、INFO、WARN、ERROR
- **函数名**: 调用日志的函数名（如果可用）
- **文件名**: 调用日志的源文件名（如果可用）
- **行号**: 调用日志的代码行号（如果可用）
- **分类**: 可选的日志分类信息
- **消息内容**: 用户提供的日志消息

输出格式类似：

```bash
2025-01-01 12:00:00.123 [INFO] [function_name@file.py:123] [category] 用户日志消息
```

## 高级日志配置

TEN framework 支持通过 `property.json` 文件配置高级日志功能。这允许您自定义日志的输出格式、目标和过滤规则。

### 配置结构

在 `property.json` 文件中，您可以在 `ten.log` 部分定义高级日志配置：

```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "debug",
              "category": "my_extension"
            }
          ],
          "formatter": {
            "type": "plain",
            "colored": true
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    }
  }
}
```

### 配置组件说明

#### 1. 日志处理器 (Handlers)

每个处理器包含三个主要组件：

- **matchers**: 定义哪些日志消息会被此处理器处理
- **formatter**: 定义日志的输出格式
- **emitter**: 定义日志的输出目标

#### 2. 匹配器 (Matchers)

匹配器用于过滤日志消息：

```json
{
  "level": "info",      // 日志级别: "off", "debug", "info", "warn", "error"
  "category": "auth"    // 可选：日志分类（字符串）
}
```

支持的日志级别：
- `"off"`: 关闭日志
- `"debug"`: 调试级别
- `"info"`: 信息级别
- `"warn"`: 警告级别
- `"error"`: 错误级别

#### 3. 格式器 (Formatter)

控制日志的输出格式：

```json
{
  "type": "plain",      // 格式类型: "plain" 或 "json"
  "colored": true       // 可选：是否启用颜色输出（仅对 plain 格式有效）
}
```

- **plain**: 人类可读的纯文本格式
- **json**: JSON 结构化格式，便于日志分析工具处理

#### 4. 发射器 (Emitter)

定义日志的输出目标：

##### 控制台输出

```json
{
  "type": "console",
  "config": {
    "stream": "stdout",  // "stdout" 或 "stderr"
    "encryption": {      // 可选：加密配置
      "enabled": true,
      "algorithm": "AES-CTR",
      "params": {
        "key": "your-encryption-key",
        "nonce": "your-nonce"
      }
    }
  }
}
```

##### 文件输出

```json
{
  "type": "file",
  "config": {
    "path": "/path/to/logfile.log",  // 日志文件路径
    "encryption": {                  // 可选：加密配置
      "enabled": true,
      "algorithm": "AES-CTR",
      "params": {
        "key": "your-encryption-key",
        "nonce": "your-nonce"
      }
    }
  }
}
```

##### OTLP 输出

OTLP (OpenTelemetry Protocol) 输出允许您将日志发送到支持 OpenTelemetry 的后端系统，如 Jaeger、Prometheus、Grafana 等。

```json
{
  "type": "otlp",
  "config": {
    "endpoint": "http://localhost:4317",  // OTLP 接收端点地址
    "protocol": "grpc",                   // 可选：传输协议，"grpc"（默认）或 "http"
    "headers": {                          // 可选：自定义 HTTP 头
      "Authorization": "Bearer token",
      "X-Custom-Header": "value"
    },
    "service_name": "my-service"          // 可选：服务名称，用于标识日志来源
  }
}
```

配置参数说明：
- **endpoint**: OTLP 接收端点的 URL（必需）
- **protocol**: 传输协议，支持 `"grpc"` 或 `"http"`，默认为 `"grpc"`
- **headers**: 自定义 HTTP 请求头，用于认证或其他元数据传递（可选）
- **service_name**: 服务名称，用于在日志聚合系统中标识不同的服务（可选）

### 配置示例

#### 示例 1: 多级别日志分离

```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "error"
            }
          ],
          "formatter": {
            "type": "json"
          },
          "emitter": {
            "type": "file",
            "config": {
              "path": "/var/log/app/errors.log"
            }
          }
        },
        {
          "matchers": [
            {
              "level": "info"
            }
          ],
          "formatter": {
            "type": "plain",
            "colored": true
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    }
  }
}
```

#### 示例 2: 按分类分离日志

```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "debug",
              "category": "database"
            }
          ],
          "formatter": {
            "type": "json"
          },
          "emitter": {
            "type": "file",
            "config": {
              "path": "/var/log/app/database.log"
            }
          }
        },
        {
          "matchers": [
            {
              "level": "info",
              "category": "api"
            }
          ],
          "formatter": {
            "type": "plain"
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    }
  }
}
```

#### 示例 3: 加密日志输出

```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "info"
            }
          ],
          "formatter": {
            "type": "json"
          },
          "emitter": {
            "type": "file",
            "config": {
              "path": "/secure/logs/app.log",
              "encryption": {
                "enabled": true,
                "algorithm": "AES-CTR",
                "params": {
                  "key": "your-32-character-encryption-key",
                  "nonce": "your-16-character-nonce"
                }
              }
            }
          }
        }
      ]
    }
  }
}
```

#### 示例 4: OTLP 日志输出到 OpenTelemetry 收集器

```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "info"
            }
          ],
          "formatter": {
            "type": "json"
          },
          "emitter": {
            "type": "otlp",
            "config": {
              "endpoint": "http://localhost:4317",
              "protocol": "grpc",
              "service_name": "ten-framework-app"
            }
          }
        },
        {
          "matchers": [
            {
              "level": "error"
            }
          ],
          "formatter": {
            "type": "json"
          },
          "emitter": {
            "type": "otlp",
            "config": {
              "endpoint": "https://api.monitoring.example.com/v1/logs",
              "protocol": "http",
              "headers": {
                "Authorization": "Bearer your-api-token",
                "X-Environment": "production"
              },
              "service_name": "ten-framework-app"
            }
          }
        }
      ]
    }
  }
}
```

### 配置注意事项

1. **多个处理器**: 可以定义多个处理器来实现复杂的日志路由策略
2. **匹配优先级**: 按照配置文件中的顺序匹配处理器
3. **加密安全**: 加密密钥和 nonce 应该安全存储，不要硬编码在配置文件中
4. **文件权限**: 确保日志文件路径具有适当的写入权限
5. **性能考虑**: JSON 格式可能会比 plain 格式产生更大的日志文件
6. **OTLP 端点**:
   - 使用 gRPC 协议时，默认端口通常为 4317
   - 使用 HTTP 协议时，默认端口通常为 4318
   - 确保 OTLP 收集器端点可访问且配置正确
7. **认证**: 使用 OTLP 时，如果后端需要认证，请在 headers 中添加相应的认证信息

## 最佳实践

1. **选择合适的日志级别**：
   - 使用 `DEBUG` 记录详细的调试信息
   - 使用 `INFO` 记录正常的业务流程
   - 使用 `WARN` 记录潜在问题
   - 使用 `ERROR` 记录错误信息

2. **提供有意义的日志消息**：
   - 包含上下文信息（如命令名称、参数值等）
   - 使用描述性的消息而不是简单的标识符
   - 合理使用 category 参数对日志进行分类

3. **避免过度日志记录**：
   - 不要在高频循环中使用过多的日志
   - 在生产环境中适当调整日志级别
   - 使用高级日志配置的匹配器来过滤不必要的日志

4. **线程安全**：
   - TEN framework 的日志 API 是线程安全的
   - 可以在任何线程中安全调用日志方法

5. **高级配置建议**：
   - 为不同的模块使用不同的 category 便于日志分析
   - 在生产环境中考虑使用文件输出以便于日志持久化
   - 对敏感日志使用加密功能
   - 定期轮转日志文件以控制磁盘使用
   - 使用 JSON 格式便于与日志分析工具集成
   - 考虑使用 OTLP 输出将日志集成到现代可观测性平台（如 Grafana、Jaeger、Datadog 等）
   - OTLP 输出与 OpenTelemetry 生态系统无缝集成，便于实现分布式追踪和日志关联

整体效果如下图所示：

![多语言 extension 的统一日志输出](https://ten-framework-assets.s3.amazonaws.com/doc-assets/log.png)
