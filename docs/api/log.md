---
title: Log
_portal_target: api/log.md
---

The TEN framework allows extensions developed in different languages to run within the same process. This creates a need to view logs from all these extensions in a unified log with a consistent format and information, making debugging easier.

To address this, the `ten_env` provides a logging API. Within each callback of an extension, an instance of `ten_env` can be accessed. Using this instance, you can call the logging API to output logs from extensions developed in various languages to a unified log output.

## Log Levels

The TEN framework supports the following log levels in increasing order of severity:

- **DEBUG** (1): Debug information
- **INFO** (2): General information
- **WARN** (3): Warning information
- **ERROR** (4): Error information

## API Interface

### C++ Language

```cpp
#include "ten_runtime/binding/cpp/ten.h"

class Extension {
public:
    void on_cmd(ten::ten_env_t& ten_env, std::unique_ptr<ten::cmd_t> cmd) {
        // Using convenience macros
        TEN_ENV_LOG_DEBUG(ten_env, "Processing command");
        TEN_ENV_LOG_INFO(ten_env, "Command name: " + cmd->get_name());
        TEN_ENV_LOG_WARN(ten_env, "Warning message");
        TEN_ENV_LOG_ERROR(ten_env, "Error occurred");

        // Direct method call with category and fields support
        ten_env.log(TEN_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__,
                   "Direct log call", nullptr, nullptr);
    }
};
```

C++ provides the following convenience macros:

- `TEN_ENV_LOG_DEBUG(ten_env, msg)`
- `TEN_ENV_LOG_INFO(ten_env, msg)`
- `TEN_ENV_LOG_WARN(ten_env, msg)`
- `TEN_ENV_LOG_ERROR(ten_env, msg)`

### Python Language

```python
from ten_runtime import Extension, TenEnv

class MyExtension(Extension):
    def on_cmd(self, ten_env: TenEnv, cmd):
        # Using different log level methods
        ten_env.log_debug("Debug information")
        ten_env.log_info("General information")
        ten_env.log_warn("Warning information")
        ten_env.log_error("Error information")

        # Supports formatted strings and additional parameters
        test_value = "example"
        ten_env.log_info(f"Processing command, test value: {test_value}")

        # Supports category and fields parameters
        ten_env.log_info("Log with category", category="my_extension")
```

Python provides the following methods:

- `log_debug(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_info(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_warn(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`
- `log_error(msg: str, category: str = None, fields: Value = None) -> Optional[TenError]`

### Go Language

```go
package main

import "ten_runtime/ten"

type MyExtension struct {
    ten.DefaultExtension
}

func (ext *MyExtension) OnCmd(tenEnv ten.TenEnv, cmd ten.Cmd) {
    // Using different log level methods
    tenEnv.LogDebug("Debug information")
    tenEnv.LogInfo("General information")
    tenEnv.LogWarn("Warning information")
    tenEnv.LogError("Error information")

    // Supports formatting
    cmdName := cmd.GetName()
    tenEnv.LogInfo("Processing command: " + cmdName)

    // Using the full Log method for additional parameters
    category := "my_extension"
    tenEnv.Log(ten.LogLevelInfo, "Log with category", &category, nil, nil)
}
```

Go provides the following methods:

- `LogDebug(msg string) error`
- `LogInfo(msg string) error`
- `LogWarn(msg string) error`
- `LogError(msg string) error`
- `Log(level LogLevel, msg string, category *string, fields *Value, option *TenLogOption) error`

### Node.js/TypeScript Language

```typescript
import { Extension, TenEnv, Cmd } from "ten-runtime-nodejs";

class MyExtension extends Extension {
    async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
        // Using different log level methods
        tenEnv.logDebug("Debug information");
        tenEnv.logInfo("General information");
        tenEnv.logWarn("Warning information");
        tenEnv.logError("Error information");

        // Supports string concatenation
        const cmdName = cmd.getName();
        tenEnv.logInfo("Processing command: " + cmdName);

        // Supports category and fields parameters
        tenEnv.logInfo("Log with category", "my_extension");
    }
}
```

Node.js provides the following methods:

- `logDebug(message: string, category?: string, fields?: Value): TenError | undefined`
- `logInfo(message: string, category?: string, fields?: Value): TenError | undefined`
- `logWarn(message: string, category?: string, fields?: Value): TenError | undefined`
- `logError(message: string, category?: string, fields?: Value): TenError | undefined`

## Usage Examples

### Basic Usage

```python
# Python example
def on_start(self, ten_env: TenEnv):
    ten_env.log_info("Extension starting")
    ten_env.on_start_done()

def on_cmd(self, ten_env: TenEnv, cmd):
    cmd_name = cmd.get_name()
    ten_env.log_debug(f"Received command: {cmd_name}")

    # Command processing logic
    if cmd_name == "hello":
        ten_env.log_info("Processing hello command", category="command_handler")
        # ... processing logic
    else:
        ten_env.log_warn(f"Unknown command: {cmd_name}")
```

### Using in Threads

```python
import threading
import time

def log_routine(self, ten_env: TenEnv):
    """Log messages in a separate thread"""
    while not self.stop_flag:
        self.log_count += 1
        ten_env.log_info(f"Log message {self.log_count}")
        time.sleep(0.05)

def on_start(self, ten_env: TenEnv):
    # Start logging thread
    self.log_thread = threading.Thread(
        target=self.log_routine, args=(ten_env,)
    )
    self.log_thread.start()
    ten_env.on_start_done()
```

### Error Handling

```python
def on_cmd(self, ten_env: TenEnv, cmd):
    try:
        # Business logic
        result = self.process_command(cmd)
        ten_env.log_info("Command processed successfully")
    except ValueError as e:
        ten_env.log_error(f"Parameter error: {str(e)}", category="error_handling")
    except Exception as e:
        ten_env.log_error(f"Fatal error: {str(e)}", category="error_handling")
        raise
```

## Log Format and Output

The TEN framework's logging system automatically includes the following information:

- **Timestamp**: Time when the log was recorded
- **Log Level**: DEBUG, INFO, WARN, ERROR
- **Function Name**: Name of the function that called the log (if available)
- **File Name**: Source file name where the log was called (if available)
- **Line Number**: Code line number where the log was called (if available)
- **Category**: Optional log category information
- **Message Content**: User-provided log message

The output format is similar to:

```bash
2025-01-01 12:00:00.123 [INFO] [function_name@file.py:123] [category] User log message
```

## Advanced Log Configuration

The TEN framework supports advanced logging functionality through the `property.json` file configuration. This allows you to customize the log output format, destination, and filtering rules.

### Configuration Structure

In the `property.json` file, you can define advanced log configuration in the `ten.log` section:

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

### Configuration Components

#### 1. Log Handlers

Each handler contains three main components:

- **matchers**: Define which log messages will be processed by this handler
- **formatter**: Define the log output format
- **emitter**: Define the log output destination

#### 2. Matchers

Matchers are used to filter log messages:

```json
{
  "level": "info",      // Log level: "off", "debug", "info", "warn", "error"
  "category": "auth"    // Optional: log category (string)
}
```

Supported log levels:

- `"off"`: Disable logging
- `"debug"`: Debug level
- `"info"`: Info level
- `"warn"`: Warning level
- `"error"`: Error level

#### 3. Formatter

Controls the log output format:

```json
{
  "type": "plain",      // Format type: "plain" or "json"
  "colored": true       // Optional: enable colored output (only for plain format)
}
```

- **plain**: Human-readable plain text format
- **json**: JSON structured format, convenient for log analysis tools

#### 4. Emitter

Defines the log output destination:

##### Console Output

```json
{
  "type": "console",
  "config": {
    "stream": "stdout",  // "stdout" or "stderr"
    "encryption": {      // Optional: encryption configuration
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

##### File Output

```json
{
  "type": "file",
  "config": {
    "path": "/path/to/logfile.log",  // Log file path
    "encryption": {                  // Optional: encryption configuration
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

##### OTLP Output

OTLP (OpenTelemetry Protocol) output allows you to send logs to OpenTelemetry-compatible backend systems, such as Jaeger, Prometheus, Grafana, and more.

```json
{
  "type": "otlp",
  "config": {
    "endpoint": "http://localhost:4317",  // OTLP receiver endpoint URL
    "protocol": "grpc",                   // Optional: transport protocol, "grpc" (default) or "http"
    "headers": {                          // Optional: custom HTTP headers
      "Authorization": "Bearer token",
      "X-Custom-Header": "value"
    },
    "service_name": "my-service"          // Optional: service name for identifying log source
  }
}
```

Configuration parameters:

- **endpoint**: OTLP receiver endpoint URL (required)
- **protocol**: Transport protocol, supports `"grpc"` or `"http"`, defaults to `"grpc"`
- **headers**: Custom HTTP request headers for authentication or metadata (optional)
- **service_name**: Service name for identifying different services in log aggregation systems (optional)

### Configuration Examples

#### Example 1: Multi-level Log Separation

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

#### Example 2: Category-based Log Separation

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

#### Example 3: Encrypted Log Output

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

#### Example 4: OTLP Log Output to OpenTelemetry Collector

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

### Configuration Notes

1. **Multiple Handlers**: You can define multiple handlers to implement complex log routing strategies
2. **Matching Priority**: Handlers are matched in the order they appear in the configuration file
3. **Encryption Security**: Encryption keys and nonces should be stored securely, not hardcoded in configuration files
4. **File Permissions**: Ensure log file paths have appropriate write permissions
5. **Performance Considerations**: JSON format may produce larger log files than plain format
6. **OTLP Endpoints**:
   - When using gRPC protocol, the default port is typically 4317
   - When using HTTP protocol, the default port is typically 4318
   - Ensure the OTLP collector endpoint is accessible and properly configured
7. **Authentication**: When using OTLP, if the backend requires authentication, add appropriate credentials in the headers

## Best Practices

1. **Choose appropriate log levels**:
   - Use `DEBUG` for detailed debugging information
   - Use `INFO` for normal business processes
   - Use `WARN` for potential issues
   - Use `ERROR` for error information

2. **Provide meaningful log messages**:
   - Include contextual information (such as command names, parameter values, etc.)
   - Use descriptive messages rather than simple identifiers
   - Use the category parameter appropriately to classify logs

3. **Avoid excessive logging**:
   - Don't use too many logs in high-frequency loops
   - Appropriately adjust log levels in production environments
   - Use advanced log configuration matchers to filter unnecessary logs

4. **Thread safety**:
   - The TEN framework's logging API is thread-safe
   - Log methods can be safely called from any thread

5. **Advanced configuration recommendations**:
   - Use different categories for different modules to facilitate log analysis
   - Consider using file output in production environments for log persistence
   - Use encryption functionality for sensitive logs
   - Regularly rotate log files to control disk usage
   - Use JSON format for easier integration with log analysis tools
   - Consider using OTLP output to integrate logs into modern observability platforms (e.g., Grafana, Jaeger, Datadog)
   - OTLP output seamlessly integrates with the OpenTelemetry ecosystem, enabling distributed tracing and log correlation

The overall effect is shown in the image below:

![Unified log output for multi-language extensions](https://ten-framework-assets.s3.amazonaws.com/doc-assets/log.png)
