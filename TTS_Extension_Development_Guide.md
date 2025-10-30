# TTS Extension 开发指南

## 概述
本教程将指导你从零开始创建一个生产级别的 TTS（Text-to-Speech） Extension，涵盖从项目创建、核心开发、测试验证到发布上线的完整流程。

### 什么是TTS Extension
TTS（Text-to-Speech）Extension 是TEN Framework中用于将文本转换为语音的核心组件。本指南将帮助开发者快速实现新的TTS供应商客户端，支持不同的通信协议和实现模式。

### 核心功能
TTS Extension的主要职责包括：
1. 接收文字：从上游Extension持续接受需要转换成语音的文字（通常来自于大模型）。
2. 请求TTS 厂商生成音频：将文字发送给TTS厂商并且接收返回的音频（通常为PCM格式）
3. 发送音频：将音频发送给下游Extension
[上游Extension]---文字--->[TTS Extension]---音频--->[下游Extension]


### 架构介绍

TTS Extension基于 `AsyncTTS2BaseExtension` 基类构建，该基类提供了：

- **异步消息队列处理**：自动讲来自上游的请求放入TTS请求队列
- **音频数据管理**：统一的音频帧格式和发送机制
- **指标上报**：TTFB（Time To First Byte）、音频时长等指标
- **错误处理**：标准化的错误报告机制
- **生命周期管理**：初始化、启动、停止、销毁的完整生命周期

### 三种实现模式对比

根据TTS供应商的API特性，我们支持以下三种实现模式：

| 模式 | 特点 | 适用场景 | 示例供应商 |
|------|------|----------|------------|
| **WebSocket双向流** | 支持实时双向通信，边发送文本边接收音频 | 需要低延迟的实时TTS | ElevenLabs |
| **WebSocket单向流** | 通过WebSocket发送请求，接收流式音频响应 | 支持流式响应的TTS服务 | Minimax |
| **HTTP流式** | 使用HTTP流式请求，适合简单的TTS服务 | 传统REST API的TTS服务 | Rime |
| **SDK集成** | 使用官方SDK，提供更丰富的功能 | 大厂提供的完整SDK | Google, Azure |

### 基类 `AsyncTTS2BaseExtension` 核心功能

```python
class AsyncTTS2BaseExtension(AsyncExtension, ABC):
    """TTS Extension基类"""
    
    # 核心方法
    @abstractmethod
    async def request_tts(self, t: TTSTextInput) -> None:
        """处理TTS请求 - 必须实现"""
        
    @abstractmethod
    def vendor(self) -> str:
        """返回供应商名称 - 必须实现"""
        
    @abstractmethod
    def synthesize_audio_sample_rate(self) -> int:
        """返回音频采样率 - 必须实现"""
    
    # 音频发送方法
    async def send_tts_audio_data(self, audio_data: bytes, timestamp: int = 0) -> None:
        """发送音频数据"""
        
    async def send_tts_audio_start(self, request_id: str, turn_id: int = -1) -> None:
        """发送音频开始事件"""
        
    async def send_tts_audio_end(self, request_id: str, request_event_interval_ms: int, 
                                request_total_audio_duration_ms: int, turn_id: int = -1, 
                                reason: TTSAudioEndReason = TTSAudioEndReason.REQUEST_END) -> None:
        """发送音频结束事件"""
        
    # 指标上报方法
    async def send_tts_ttfb_metrics(self, request_id: str, ttfb_ms: int, turn_id: int = -1) -> None:
        """发送TTFB指标"""
        
    # 错误处理
    async def send_tts_error(self, request_id: str | None, error: ModuleError) -> None:
        """发送错误信息"""
```

### 实现模式选择指南

**选择WebSocket双向流模式当：**
- 供应商支持实时双向通信
- 需要极低延迟的TTS响应
- 支持流式文本输入和音频输出

**选择WebSocket单向流模式当：**
- 供应商支持WebSocket但只支持单向流
- 需要保持长连接以降低延迟
- 支持流式音频响应

**选择HTTP模式当：**
- 供应商只提供REST API
- 不需要保持长连接
- 实现简单，维护成本低

**选择SDK模式当：**
- 供应商提供官方SDK
- 需要利用SDK的高级功能
- 追求稳定性和功能完整性

## 项目初始化
使用 TMan 的 TTS 专用模板快速创建项目骨架：

### 进入扩展目录
cd ten-framework/ai_agents/agents/ten_packages/extension

### 创建TTS扩展项目
tman create extension my_tts_extension --template default_tts_python --template-data class_name_prefix=MyTts
创建成功后会显示：

Package 'extension:my_tts_extension' created successfully in 'my_tts_extension' in 2 seconds.

###安装项目依赖
先在 requirements.txt 中添加 所需依赖：

###安装 TEN 依赖
进入创建的扩展目录并安装依赖：

cd my_asr_extension
tman install --standalone
这会根据 manifest.json 中声明的依赖构建依赖树，并安装到 .ten 目录下。

## 项目结构规范

### 必需文件列表

每个TTS Extension项目必须包含以下文件：

```
{vendor}_tts_python/
├── __init__.py                    # Python包初始化文件
├── config.py                      # 配置管理类
├── {vendor}_tts.py                # TTS客户端核心实现
├── extension.py                   # Extension业务逻辑控制(tman 创建Extension时生成)
├── manifest.json                  # Extension元数据配置(tman 创建Extension时生成)
├── property.json                  # 默认配置参数(tman 创建Extension时生成)
├── requirements.txt               # Python依赖管理
├── README.md                      # 项目说明文档
└── tests/                         # 测试目录(推荐包含的最基本测试)
    ├── __init__.py
    ├── conftest.py               # pytest配置和fixtures
    ├── test_basic.py             # 基础功能ut测试
    ├── test_error_msg.py         # 错误处理ut测试
    ├── test_params.py            # 参数配置ut测试
    ├── test_robustness.py        # 健壮性ut测试
    └── configs/                  # 测试配置文件，用于guarder 测试
        └── test_config.json
```

### 文件命名规范

- **目录名**：`{vendor}_tts_python` 或 `{vendor}_tts2_python`（v2版本）
- **核心文件**：`{vendor}_tts.py` 或 `{vendor}_tts2.py`
- **配置类**：`{Vendor}TTSConfig` 或 `{Vendor}TTS2Config`
- **客户端类**：`{Vendor}TTS` 或 `{Vendor}TTS2`
- **Extension类**：`{Vendor}TTS Extension` 或 `{Vendor}TTS2Extension`

### 文件作用说明

#### 1. `__init__.py`
```python
# 包初始化文件，通常为空或包含版本信息
__version__ = "0.1.0"
```

#### 2. `config.py`
- 定义配置类，继承自 `pydantic.BaseModel`
- 处理敏感信息加密
- 参数验证和更新机制
- 供应商特定参数映射
- 参数黑名单控制

#### 3. `{vendor}_tts.py`
- 实现TTS客户端核心逻辑
- 处理与供应商API的通信
- 定义事件系统和错误处理
- 管理连接生命周期

#### 4. `extension.py`
- 继承 `AsyncTTS2BaseExtension`
- 实现业务逻辑控制
- 处理音频数据流
- 管理请求生命周期

#### 5. `manifest.json`
- Extension元数据定义
- API接口声明
- 依赖关系配置
- 属性schema定义

#### 6. `property.json`
- 默认配置参数
- 环境变量支持
- 供应商特定配置

#### 7. `requirements.txt`
- Python依赖包列表
- 版本约束
- 供应商SDK依赖

#### 8. `tests/` 目录
- 完整的测试套件
- Mock测试支持
- 配置文件测试
- 错误场景测试

### 目录结构最佳实践

1. **保持一致性**：所有TTS Extension应遵循相同的目录结构
2. **模块化设计**：将不同功能分离到不同文件
3. **测试覆盖**：确保每个功能都有对应的测试
4. **文档完整**：README.md应包含使用说明和配置示例
5. **版本管理**：使用语义化版本号，在manifest.json中声明

## WebSocket双向流模式规范

### 模式特点

WebSocket双向流模式支持实时双向通信，允许边发送文本边接收音频数据，实现最低延迟的TTS响应。典型实现如ElevenLabs TTS2。

### 核心架构

```
┌─────────────────┐    WebSocket     ┌─────────────────┐
│   Extension     │◄─────────────────►│  TTS Provider   │
│                 │  双向流通信        │                 │
│ - 文本输入队列    │                  │ - 实时音频生成   │
│ - 音频输出处理    │                  │ - 流式响应      │
│ - 连接管理       │                  │ - 错误处理      │
└─────────────────┘                  └─────────────────┘
```

### 实现规范

#### 1. 连接管理策略

**连接生命周期管理：**
```python
class VendorTTS2Synthesizer:
    def __init__(self, config, ten_env, error_callback, response_msgs):
        self.config = config
        self.ten_env = ten_env
        self.error_callback = error_callback
        self.response_msgs = response_msgs
        
        # 连接状态管理
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        self.channel_tasks = []
        self._session_started = False
        
        # 事件同步机制
        self._connection_event = asyncio.Event()
        self._connection_success = False
        self._receive_ready_event = asyncio.Event()
        
        # 启动WebSocket连接监控
        self.websocket_task = asyncio.create_task(self._process_websocket())
```

**自动重连机制：**
```python
async def _process_websocket(self) -> None:
    """主WebSocket连接监控和重连逻辑"""
    try:
        # 使用websockets.connect的自动重连机制
        async for ws in websockets.connect(
            uri=self.uri,
            process_exception=self._process_ws_exception,
            max_size=1024 * 1024 * 16,
        ):
            self.ws = ws
            try:
                # 启动发送和接收任务
                self.channel_tasks = [
                    asyncio.create_task(self._send_loop(ws)),
                    asyncio.create_task(self._receive_loop(ws)),
                ]
                
                # 等待接收循环准备就绪
                await self._receive_ready_event.wait()
                await self.start_connection()
                
                await self._await_channel_tasks()
                
            except websockets.ConnectionClosed as e:
                if not self._session_closing:
                    # 重置所有事件状态
                    self._receive_ready_event.clear()
                    self._connection_event.clear()
                    self._connection_success = False
                    self._session_started = False
                    continue
    except Exception as e:
        self.ten_env.log_error(f"WebSocket connection error: {e}")
```

#### 2. 消息队列处理

**文本输入队列：**
```python
async def _send_loop(self, ws: ClientConnection) -> None:
    """文本发送循环"""
    try:
        # 发送初始化消息
        init_msg = ...
        await ws.send(json.dumps(init_msg))
        
        while not self._session_closing:
            try:
                # 从队列获取文本数据, timeout时间 < TTS供应商设置的无情求断链时间(Elevenlabs 是20 秒，这里就设置了18秒)
                text_data = await asyncio.wait_for(
                    self.text_input_queue.get(), timeout=18
                )
            except asyncio.TimeoutError:
                # 发送空文本保持连接，每个厂商的保持链接请求不同，可查看厂商文档确认
                await ws.send(json.dumps({"text": " "}))
                continue
                
            if text_data is None:
                continue
                
            if text_data.text.strip() != "":
                #避免发送无意义请求。或者避免再一轮请求中间发送断链请求。
                #Elevenlabs定义“”空字符串为断链请求。
                await ws.send(json.dumps({"text": text_data.text}))
                
            if text_data.text_input_end == True: 
                #text_input_end True标识当前的请求结束，此时可以发送主动断链请求，也可不发送。根据自己需求判断。
                #每个厂商断链请求不同，需参考厂商文档。
                await ws.send(json.dumps({"text": ""}))
                return
                
    except asyncio.CancelledError:
        raise
    except Exception as e:
        self.ten_env.log_error(f"Exception in send_loop: {e}")
        raise
```

**音频接收处理：**
```python
async def _receive_loop(self, ws: ClientConnection) -> None:
    """消息接收循环"""
    try:
        self._receive_ready_event.set()
        
        while self._session_closing == False:
            message = await ws.recv()
            #解析response
                
    except asyncio.CancelledError:
        raise
    except Exception as e:
        self.ten_env.log_error(f"Exception in receive_loop: {e}")
        raise
```

#### 3. 错误处理和重连机制

**异常处理策略：**
```python
def _process_ws_exception(self, exp) -> None | Exception:
    """处理WebSocket连接异常并决定是否重连"""
    self.ten_env.log_debug(f"Websocket internal error: {exp}")
    self._connect_exp_cnt += 1
    
    if self._connect_exp_cnt > 5:  # 最大重试次数
        self.ten_env.log_error(f"Max retries exceeded: {str(exp)}")
        return exp
    return None  # 继续重连
```

**错误回调处理：**
```python
error_info = ModuleErrorVendorInfo(
    vendor="vendor_name",
    code=str(data.get("code", 0)),
    message=data.get("error", "Unknown error"),
)
error_code = ModuleErrorCode.NON_FATAL_ERROR #不可恢复错误，如key等实效

error_code = ModuleErrorCode.FATAL_ERROR #可恢复错误，如当前请求无法识别
    
if self.error_callback:
    module_error = ModuleError(
        message=data["error"],
        module=ModuleType.TTS,
        code=error_code,
        vendor_info=error_info,
    )
    await self.error_callback("", module_error)
```

#### 4. 资源管理和清理

**连接取消和清理：**
```python
def cancel(self) -> None:
    """取消当前连接，用于flush场景"""
    self.ten_env.log_info("Cancelling the request.")
    
    self._session_closing = True
    
    # 取消WebSocket任务
    if self.websocket_task:
        self.websocket_task.cancel()
    
    # 取消所有通道任务
    for task in self.channel_tasks:
        task.cancel()
    
    # 清理所有队列
    self._clear_queues()

def _clear_queues(self) -> None:
    """清理所有队列以防止旧数据处理"""
    # 清理文本队列
    while not self.text_input_queue.empty():
        try:
            self.text_input_queue.get_nowait()
        except asyncio.QueueEmpty:
            break
    
    # 清理响应消息队列
    if self.response_msgs:
        while not self.response_msgs.empty():
            try:
                self.response_msgs.get_nowait()
            except asyncio.QueueEmpty:
                break
```

### 实现模板

基于ElevenLabs TTS2的完整实现模板：

```python
class VendorTTS2Synthesizer:
    """WebSocket双向流TTS合成器"""
    
    def __init__(self, config, ten_env, error_callback, response_msgs):
        # 初始化代码...
        
    async def _process_websocket(self) -> None:
        # WebSocket连接管理...
        
    async def _send_loop(self, ws: ClientConnection) -> None:
        # 文本发送循环...
        
    async def _receive_loop(self, ws: ClientConnection) -> None:
        # 音频接收循环...
        
    async def start_connection(self):
        # 建立连接...
        
    async def send_text(self, text_data):
        # 发送文本...
        
    def cancel(self) -> None:
        # 取消连接...
        
    async def close(self):
        # 关闭连接...

class VendorTTS2Client:
    """TTS客户端管理器"""
    
    def __init__(self, config, ten_env, error_callback, response_msgs):
        self.synthesizer = self._create_synthesizer()
        self.cancelled_synthesizers = []
        self.cleanup_task = asyncio.create_task(
            self._cleanup_cancelled_synthesizers()
        )
    
    def cancel(self) -> None:
        # 取消当前合成器并创建新的...
        
    async def send_text(self, text_data):
        # 发送文本...
        
    async def close(self):
        # 关闭客户端...
```

### 最佳实践

1. **连接预热**：在初始化时建立WebSocket连接，减少首次请求延迟
2. **自动重连**：实现指数退避的重连策略
3. **资源清理**：及时清理取消的合成器，避免内存泄漏
4. **错误分类**：区分网络错误、认证错误和业务错误
5. **队列管理**：使用有界队列防止内存溢出
6. **超时处理**：设置合理的超时时间，避免长时间阻塞

## WebSocket单向流模式规范

### 模式特点

WebSocket单向流模式通过WebSocket发送请求，接收流式音频响应。与双向流模式不同，它采用请求-响应模式，适合支持WebSocket但只提供单向流的TTS服务。典型实现如Minimax TTS。

### 核心架构

```
┌─────────────────┐    WebSocket     ┌─────────────────┐
│   Extension     │─────────────────►│  TTS Provider   │
│                 │  发送请求         │                 │
│ - 请求管理       │                  │ - 流式音频响应   │
│ - 响应处理       │◄─────────────────│ - 任务状态管理   │
│ - 连接复用       │  接收响应         │ - 错误处理      │
└─────────────────┘                  └─────────────────┘
```

### 实现规范

#### 1. 连接生命周期管理

**WebSocket资源管理：**
```python
class VendorTTSWebsocket:
    def __init__(self, config, ten_env, vendor, error_callback):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.error_callback = error_callback
        
        # 状态管理
        self.stopping: bool = False
        self.discarding: bool = False
        self.first_chunk_processed: bool = False
        self.ws: websockets.ClientConnection | None = None
        
        # 自动重连管理
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        
        # WebSocket资源管理
        self.ws_released_event: asyncio.Event = asyncio.Event()
        self.ws_released_event.set()  # 初始时没有活跃的WS
        self.stopped_event: asyncio.Event = asyncio.Event()
        
        # 启动WebSocket连接（立即开始连接）
        self.websocket_task = asyncio.create_task(self._process_websocket())
```

**连接启动和停止：**
```python

async def stop(self):
    """停止并清理WebSocket连接"""
    self.stopping = True
    self._session_closing = True
    self.first_chunk_processed = False
    await self.cancel()
    
    # 停止WebSocket任务
    if self.websocket_task:
        self.websocket_task.cancel()
        try:
            await self.websocket_task
        except asyncio.CancelledError:
            pass
    
    # 等待处理器退出
    await self.stopped_event.wait()

async def cancel(self):
    """取消当前操作并等待资源释放"""
    if self.ten_env:
        self.ten_env.log_info("Cancelling TTS operations")
    
    if self.discarding:
        return  # 已经在取消中
    
    self.discarding = True
    self.first_chunk_processed = False
    
    # 等待WS资源被释放
    await self.ws_released_event.wait()
```

**自动重连处理：**
```python
async def _process_websocket(self) -> None:
    """WebSocket连接监控和重连逻辑
    
    这个方法在__init__中被启动后立即开始连接循环。
    使用websockets.connect的自动重连机制，在连接断开时自动重试。
    """
    try:
        async for ws in websockets.connect(
            uri=self._get_websocket_uri(),
            process_exception=self._process_ws_exception,
            max_size=1024 * 1024 * 16,
        ):
            self.ws = ws
            try:
                await self._await_connection_tasks()
                
            except websockets.ConnectionClosed as e:
                if not self._session_closing:
                    # 继续连接循环进行重连
                    continue
                else:
                    # 主动关闭，退出循环
                    break
    except Exception as e:
        if not self._session_closing:
            self.ten_env.log_error(f"WebSocket connection error: {e}")
            # 异常也会触发websockets.connect的重连机制

def _get_websocket_uri(self) -> str:
    """获取WebSocket URI - 子类实现"""
    raise NotImplementedError("Subclasses must implement _get_websocket_uri")

def _process_ws_exception(self, ws, exc):
    """处理WebSocket异常"""
    self.ten_env.log_error(f"WebSocket exception: {exc}")
    return ws.close(code=1011, reason=str(exc))
```

#### 2. 请求-响应处理

**TTS请求处理：**
```python
async def get(self, text: str) -> AsyncIterator[tuple[Union[bytes, int, None], int | None]]:
    """为给定文本生成TTS音频，返回(data, event_status)"""
    if not text or text.strip() == "":
        return
    
    if not self.ws:
        yield "WebSocket not connected".encode(), EVENT_TTS_ERROR, None
        return
    
    self.discarding = False
    
    try:
        # 发送TTS请求
        request_data = self._create_request_data(text)
        await self.ws.send(json.dumps(request_data))
        
        # 处理响应
        async for response in self._receive_responses():
            if self.discarding:
                yield None, EVENT_TTS_FLUSH, None
                break
            
            audio_data, event_type, ttfb_ms = self._parse_response(response)
            yield audio_data, event_type, ttfb_ms
            
            if event_type == EVENT_TTS_REQUEST_END:
                break
                
    except Exception as e:
        self.ten_env.log_error(f"TTS request failed: {e}")
        yield str(e).encode("utf-8"), EVENT_TTS_ERROR, None

def _create_request_data(self, text: str) -> dict:
    """创建请求数据 - 子类实现"""
    raise NotImplementedError("Subclasses must implement _create_request_data")

def _parse_response(self, response: str) -> tuple:
    """解析响应数据 - 子类实现"""
    raise NotImplementedError("Subclasses must implement _parse_response")

async def _receive_responses(self) -> AsyncIterator[str]:
    """接收响应流 - 子类实现"""
    raise NotImplementedError("Subclasses must implement _receive_responses")
```

#### 3. 任务状态管理

**任务生命周期跟踪：**
```python
class VendorTTSTaskFailedException(Exception):
    """TTS任务失败异常"""
    
    def __init__(self, error_msg: str, error_code: int):
        self.error_msg = error_msg
        self.error_code = error_code
        super().__init__(f"TTS task failed: {error_msg} (code: {error_code})")

# 任务状态事件定义
EVENT_TTSSentenceStart = 350
EVENT_TTSSentenceEnd = 351
EVENT_TTSResponse = 352
EVENT_TTSTaskFinished = 353
EVENT_TTSFlush = 354
EVENT_TTS_TTFB_METRIC = 355
```

**任务完成处理：**
```python
async def _handle_task_completion(self, data: dict):
    """处理任务完成事件"""
    if data.get("status") == "success":
        self.ten_env.log_info("TTS task completed successfully")
    elif data.get("status") == "failed":
        error_msg = data.get("error_message", "Task failed")
        error_code = data.get("error_code", 0)
        raise VendorTTSTaskFailedException(error_msg, error_code)
```

#### 4. 错误处理和重试机制

**错误分类处理：**
```python
def _handle_error_response(self, data: dict):
    """处理错误响应"""
    error_code = data.get("code", 0)
    error_message = data.get("message", "Unknown error")
    
    # 根据错误代码分类处理，根据TTS厂商返回值分类，此处以mininmax为例
    if error_code in [1001, 1002]:  # 认证错误
        raise VendorTTSTaskFailedException(
            f"Authentication failed: {error_message}", error_code
        )
    elif error_code in [2001, 2002]:  # 参数错误
        raise VendorTTSTaskFailedException(
            f"Invalid parameters: {error_message}", error_code
        )
    elif error_code in [3001, 3002]:  # 服务错误
        raise VendorTTSTaskFailedException(
            f"Service error: {error_message}", error_code
        )
    else:
        raise VendorTTSTaskFailedException(
            f"Unknown error: {error_message}", error_code
        )
```

**重试机制：**
```python
async def get_with_retry(self, text: str, max_retries: int = 3) -> AsyncIterator[tuple[Union[bytes, int, None], int | None]]:
    """带重试的TTS请求"""
    retry_count = 0
    
    while retry_count < max_retries:
        try:
            async for result in self.get(text):
                yield result
            return  # 成功完成，退出重试循环
            
        except VendorTTSTaskFailedException as e:
            retry_count += 1
            if retry_count >= max_retries:
                self.ten_env.log_error(f"TTS request failed after {max_retries} retries: {e}")
                yield str(e).encode("utf-8"), EVENT_TTS_ERROR
                return
            
            # 等待后重试
            await asyncio.sleep(2 ** retry_count)  # 指数退避
            self.ten_env.log_info(f"Retrying TTS request (attempt {retry_count + 1}/{max_retries})")
```

### 实现模板

基于Minimax TTS的完整实现模板：

```python
class VendorTTSWebsocket:
    """WebSocket单向流TTS客户端"""
    
    def __init__(self, config, ten_env, vendor, error_callback):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.error_callback = error_callback
        
        # 状态管理
        self.stopping: bool = False
        self.discarding: bool = False
        self.first_chunk_processed: bool = False
        self.ws: websockets.ClientConnection | None = None
        
        # 自动重连管理
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        
        # 事件同步
        self._connection_event = asyncio.Event()
        self._connection_success = False
        
        # WebSocket资源管理
        self.ws_released_event: asyncio.Event = asyncio.Event()
        self.ws_released_event.set()
        self.stopped_event: asyncio.Event = asyncio.Event()
        
        # 启动WebSocket连接
        self.websocket_task = asyncio.create_task(self._process_websocket())
        
    async def start(self):
        """启动WebSocket处理器"""
        if self.ten_env:
            self.ten_env.log_info("Starting VendorTTSWebsocket processor")
        self._connection_event.set()
        
    async def stop(self):
        """停止WebSocket处理器"""
        self.stopping = True
        self._session_closing = True
        self.first_chunk_processed = False
        await self.cancel()
        
        if self.websocket_task:
            self.websocket_task.cancel()
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                pass
        
        await self.stopped_event.wait()
        
    async def cancel(self):
        """取消当前操作"""
        if self.ten_env:
            self.ten_env.log_info("Cancelling TTS operations")
        
        if self.discarding:
            return
        
        self.discarding = True
        self.first_chunk_processed = False
        await self.ws_released_event.wait()
        
    async def get(self, text: str) -> AsyncIterator[tuple[Union[bytes, int, None], int | None]]:
        """处理TTS请求"""
        if not text or text.strip() == "":
            return
        
        if not self.ws:
            yield "WebSocket not connected".encode(), EVENT_TTS_ERROR, None
            return
        
        self.discarding = False
        
        try:
            # 发送TTS请求
            request_data = self._create_request_data(text)
            await self.ws.send(json.dumps(request_data))
            
            # 处理响应
            async for response in self._receive_responses():
                if self.discarding:
                    yield None, EVENT_TTS_FLUSH, None
                    break
                
                audio_data, event_type, ttfb_ms = self._parse_response(response)
                yield audio_data, event_type, ttfb_ms
                
                if event_type == EVENT_TTS_REQUEST_END:
                    break
                    
        except Exception as e:
            self.ten_env.log_error(f"TTS request failed: {e}")
            yield str(e).encode("utf-8"), EVENT_TTS_ERROR, None
        
    def _get_websocket_uri(self) -> str:
        """获取WebSocket URI - 子类实现"""
        raise NotImplementedError("Subclasses must implement _get_websocket_uri")
    
    def _create_request_data(self, text: str) -> dict:
        """创建请求数据 - 子类实现"""
        raise NotImplementedError("Subclasses must implement _create_request_data")
    
    def _parse_response(self, response: str) -> tuple:
        """解析响应数据 - 子类实现"""
        raise NotImplementedError("Subclasses must implement _parse_response")
    
    async def _receive_responses(self) -> AsyncIterator[str]:
        """接收响应流 - 子类实现"""
        raise NotImplementedError("Subclasses must implement _receive_responses")
    
    def _process_ws_exception(self, ws, exc):
        """处理WebSocket异常"""
        self.ten_env.log_error(f"WebSocket exception: {exc}")
        return ws.close(code=1011, reason=str(exc))

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """TTS Extension实现"""
    
    def __init__(self, name: str) -> None:
        super().__init__(name)
        # 初始化代码...
        
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        # 初始化代码...
        
    async def request_tts(self, t: TTSTextInput) -> None:
        # TTS请求处理...
        
    def vendor(self) -> str:
        return "vendor_name"
        
    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate
```

### 最佳实践

1. **自动重连**：实现WebSocket连接监控和自动重连机制，提高服务稳定性
2. **连接复用**：使用连接池或连接复用机制，减少连接建立开销
3. **资源管理**：及时释放WebSocket资源，避免资源泄漏
4. **任务跟踪**：维护任务状态，支持任务取消和重试
5. **错误分类**：根据错误类型采用不同的处理策略
6. **超时控制**：设置合理的超时时间，避免长时间等待
7. **重试策略**：实现指数退避的重试机制
8. **状态同步**：使用事件机制同步连接状态
9. **抽象设计**：使用抽象方法分离厂商特定逻辑，提高代码复用性

## HTTP模式规范

### 模式特点

HTTP模式使用标准的HTTP流式请求，适合传统的REST API TTS服务。实现简单，维护成本低，但延迟相对较高。典型实现如Rime TTS。

### 核心架构

```
┌─────────────────┐    HTTP Stream    ┌─────────────────┐
│   Extension     │─────────────────►│  TTS Provider    │
│                 │  POST请求          │                 │
│ - HTTP客户端     │                  │ - REST API       │
│ - 流式响应处理    │◄─────────────────│ - 流式音频响应    │
│ - 错误重试       │  Stream Response  │ - 错误处理       │
└─────────────────┘                  └─────────────────┘
```

### 实现规范

#### 1. HTTP客户端配置

**客户端初始化：**
```python
class VendorTTSClient:
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.api_key = config.api_key
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        
        # API端点配置
        self.endpoint = "https://api.vendor.com/v1/tts"
        
        # 请求头配置
        self.headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
            "Accept": "audio/pcm",
        }
        
        # HTTP客户端配置
        self.client = AsyncClient(
            http2=True,  # 启用HTTP/2
            follow_redirects=True,
            ......
        )
```

**连接池管理：**
```python
async def stop(self):
    """停止HTTP客户端"""
    if self.client:
        self.ten_env.log_debug("Stopping HTTP client")
        await self.client.aclose()
        self.client = None

def cancel(self):
    """取消当前请求"""
    self.ten_env.log_debug("VendorTTS: cancel() called.")
    self._is_cancelled = True
```

#### 2. 流式请求处理

**TTS请求处理：**
```python
async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int | None]]:
    """处理单个TTS请求"""
    self._is_cancelled = False
    
    if not self.client:
        return
    
    try:
        # 构建请求数据
        request_data = {
            "text": text,
            **self.config.params,
        }
        
        # 发送流式请求
        async with self.client.stream(
            "POST",
            self.endpoint,
            headers=self.headers,
            json=request_data,
        ) as response:
            # 检查响应状态
            if response.status_code != 200:
                error_msg = f"HTTP {response.status_code}: {response.reason_phrase}"
                yield error_msg.encode("utf-8"), EVENT_TTS_ERROR
                return
            
            # 处理流式响应
            async for chunk in response.aiter_bytes(chunk_size=4096):
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        "Cancellation flag detected, sending flush event and stopping TTS stream."
                    )
                    yield None, EVENT_TTS_FLUSH
                    break
                
                self.ten_env.log_debug(
                    f"VendorTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)}"
                )
                
                if len(chunk) > 0:
                    yield bytes(chunk), EVENT_TTS_RESPONSE
                else:
                    yield None, EVENT_TTS_END
            
            # 请求完成
            if not self._is_cancelled:
                self.ten_env.log_debug("VendorTTS: sending EVENT_TTS_END")
                yield None, EVENT_TTS_END
    
    except Exception as e:
        # 错误处理
        error_message = str(e)
        self.ten_env.log_error(f"vendor_error: {error_message}")
        
        # 检查错误类型
        if self._is_authentication_error(error_message):
            yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR
        else:
            yield error_message.encode("utf-8"), EVENT_TTS_ERROR
```

#### 3. 错误处理和重试机制

**错误分类：**
```python
def _is_authentication_error(self, error_message: str) -> bool:
    """检查是否为认证错误"""
    auth_keywords = [
        "401", "unauthorized", "authentication", "credentials",
        "invalid token", "expired token", "forbidden"
    ]
    return any(keyword in error_message.lower() for keyword in auth_keywords)

def _is_network_error(self, error_message: str) -> bool:
    """检查是否为网络错误"""
    network_keywords = [
        "timeout", "connection", "network", "dns", "socket",
        "connection refused", "connection reset"
    ]
    return any(keyword in error_message.lower() for keyword in network_keywords)

def _is_retryable_error(self, error_message: str) -> bool:
    """检查是否为可重试错误"""
    retryable_keywords = [
        "timeout", "connection", "503", "502", "504",
        "service unavailable", "bad gateway", "gateway timeout"
    ]
    return any(keyword in error_message.lower() for keyword in retryable_keywords)
```

**重试机制：**
```python
async def get_with_retry(self, text: str, max_retries: int = 3) -> AsyncIterator[tuple[bytes | None, int | None]]:
    """带重试的TTS请求"""
    retry_count = 0
    base_delay = 1.0
    
    while retry_count < max_retries:
        try:
            async for result in self.get(text):
                yield result
            return  # 成功完成
            
        except Exception as e:
            error_message = str(e)
            
            if not self._is_retryable_error(error_message):
                # 不可重试的错误，直接返回
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR
                return
            
            retry_count += 1
            if retry_count >= max_retries:
                self.ten_env.log_error(f"TTS request failed after {max_retries} retries: {error_message}")
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR
                return
            
            # 指数退避延迟
            delay = base_delay * (2 ** (retry_count - 1))
            self.ten_env.log_info(f"Retrying TTS request in {delay}s (attempt {retry_count + 1}/{max_retries})")
            await asyncio.sleep(delay)
```

#### 4. 请求优化

**连接复用：**
```python
class VendorTTSClient:
    def __init__(self, config, ten_env):
        # 使用连接池
        self.client = AsyncClient(
            timeout=Timeout(
                connect=10.0,  # 连接超时
                read=30.0,     # 读取超时
                write=10.0,    # 写入超时
                pool=5.0       # 连接池超时
            ),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
            follow_redirects=True,
        )
```

**请求压缩：**
```python
def _get_headers(self) -> dict:
    """获取优化的请求头"""
    return {
        "Authorization": f"Bearer {self.api_key}",
        "Content-Type": "application/json",
        "Accept": "audio/pcm",
        "Accept-Encoding": "gzip, deflate",  # 启用压缩
        "User-Agent": "TEN-Framework-TTS/1.0",
    }
```

### 实现模板

基于Rime TTS的完整实现模板：

```python
class VendorTTSClient:
    """HTTP流式TTS客户端"""
    
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        # 初始化代码...
        
    async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """处理TTS请求"""
        # 请求处理代码...
        
    async def get_with_retry(self, text: str, max_retries: int = 3) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """带重试的TTS请求"""
        # 重试逻辑代码...
        
    def cancel(self):
        """取消当前请求"""
        # 取消代码...
        
    async def stop(self):
        """停止客户端"""
        # 停止代码...
        
    def _is_authentication_error(self, error_message: str) -> bool:
        """检查认证错误"""
        # 错误检查代码...
        
    def _is_retryable_error(self, error_message: str) -> bool:
        """检查可重试错误"""
        # 错误检查代码...

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """TTS Extension实现"""
    
    def __init__(self, name: str) -> None:
        super().__init__(name)
        # 初始化代码...
        
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        # 初始化代码...
        
    async def request_tts(self, t: TTSTextInput) -> None:
        # TTS请求处理...
        
    def vendor(self) -> str:
        return "vendor_name"
        
    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate
```

### 最佳实践

1. **连接池管理**：使用HTTP连接池，减少连接建立开销
2. **超时控制**：设置合理的连接、读取、写入超时时间
3. **重试策略**：实现指数退避的重试机制
4. **错误分类**：区分认证错误、网络错误和业务错误
5. **压缩支持**：启用HTTP压缩，减少传输数据量
6. **HTTP/2支持**：使用HTTP/2协议，提高性能
7. **资源清理**：及时关闭HTTP客户端，释放资源
8. **请求优化**：使用适当的chunk大小，平衡内存使用和性能

## SDK模式规范

### 模式特点

SDK模式使用供应商提供的官方SDK，提供更丰富的功能和更好的稳定性。适合大厂提供的完整SDK，如Google Cloud TTS、Azure Cognitive Services等。

### 核心架构

```
┌─────────────────┐    SDK API        ┌─────────────────┐
│   Extension     │─────────────────►│  Vendor SDK      │
│                 │  异步调用          │                 │
│ - SDK客户端      │                  │ - 官方SDK        │
│ - 流式处理       │◄─────────────────│ - 流式合成       │
│ - 配置映射       │  音频流           │ - 错误处理       │
└─────────────────┘                  └─────────────────┘
```

### 实现规范

#### 1. SDK集成方式

**SDK客户端初始化：**
```python
class VendorTTS:
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.client = None
        self._initialize_client()
        self.send_text_in_connection = False
        self.cur_request_id = ""
        self.streaming_enabled = False
        self.streaming_config = None
        self._should_stop_streaming = False

    def _initialize_client(self):
        """初始化供应商SDK客户端"""
        try:
            # 解析认证信息
            credentials = self._parse_credentials()
            
            # 创建SDK客户端
            self.client = vendor_sdk.TextToSpeechClient(
                credentials=credentials
            )
            
            self.ten_env.log_debug("Vendor TTS client initialized successfully")
            
        except Exception as e:
            self.ten_env.log_error(f"Failed to initialize Vendor TTS client: {e}")
            raise

```

#### 2. 流式合成处理

**流式配置创建：**
```python
def _create_streaming_config(self) -> vendor_sdk.StreamingSynthesizeConfig:
    """从配置参数创建流式配置"""
    """根据厂商行为，讲参数设置到对应地方。保证参数透传，黑名单，白名单"""
```

**请求生成器：**
```python
def _create_request_generator(self, text: str) -> Iterator[vendor_sdk.RequestFunction]:
    """为流式合成创建请求生成器"""
    # 第一个请求包含配置
    config_request = vendor_sdk.RequestFunction(
        streaming_config=self.streaming_config
    )
    yield config_request
    
    # 发送文本块
    text_chunks = [text]  # 可以优化为分割成更小的块
    
    for chunk in text_chunks:
        if chunk.strip():  # 只发送非空块
            yield vendor_sdk.RequestFunction(
                input=vendor_sdk.StreamingSynthesisInput(text=chunk)
            )
```

#### 3. 异步处理策略

**异步生成器实现：**
```python
async def get_streaming(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
    """使用流式合成生成TTS音频"""
    
    if not self.client:
        error_msg = "Vendor TTS client not initialized"
        self.ten_env.log_error(error_msg)
        yield error_msg.encode("utf-8"), EVENT_TTS_ERROR
        return
    
    # 初始化流式配置
    if not self.streaming_config:
        self.streaming_config = self._create_streaming_config()
    
    # 重试配置
    max_retries = 3
    retry_delay = 1.0
    
    # 重试循环
    for attempt in range(max_retries):
        ttfb_ms = None
        try:
            start_ts = None
            if request_id != self.cur_request_id:
                start_ts = time.time()
                self.cur_request_id = request_id
            
            # 创建请求生成器
            request_generator = self._create_request_generator(text)
            
            # 执行流式文本转语音请求
            streaming_responses = self.client.streaming_synthesize(request_generator)
            
            if start_ts is not None:
                ttfb_ms = int((time.time() - start_ts) * 1000)
            self.send_text_in_connection = True
            
            # 处理流式响应
            audio_received = False
            for response in streaming_responses:
                # 检查是否应该停止流式处理
                if self._should_stop_streaming:
                    self.ten_env.log_debug("Stopping streaming synthesis due to flush request")
                    break
                
                if response.audio_content:
                    audio_received = True
                    yield response.audio_content, EVENT_TTS_RESPONSE, ttfb_ms
                    # 第一次后重置ttfb_ms
                    ttfb_ms = None
            
            if audio_received:
                yield None, EVENT_TTS_REQUEST_END, ttfb_ms
                return  # 成功，退出重试循环
            else:
                error_msg = "No audio content received from Vendor TTS streaming"
                yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
                return
        
        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(f"vendor_error: {error_message}")
            
            # 检查是否为可重试的网络错误
            is_retryable = self._is_retryable_error(error_message)
            
            if is_retryable and attempt < max_retries - 1:
                self.ten_env.log_debug(f"Network error (attempt {attempt + 1}/{max_retries}): {error_message}")
                await asyncio.sleep(retry_delay)
                retry_delay *= 2  # 指数退避
                continue
            else:
                # 最终尝试失败或不可重试错误
                if self._is_authentication_error(error_message):
                    yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR, ttfb_ms
                else:
                    yield error_message.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
                return
```

#### 4. 资源管理和清理

**客户端清理和重置：**
```python
def clean(self):
    """清理资源"""
    self.ten_env.log_info("VendorTTS: clean() called.")
    # 设置标志停止任何正在进行的流式处理
    self._should_stop_streaming = True
    if self.client:
        self.client = None
        self.ten_env.log_debug("Vendor TTS client cleaned")
    # 重置流式配置
    self.streaming_config = None

async def reset(self):
    """重置客户端"""
    self.ten_env.log_info("Resetting Vendor TTS client")
    self.client = None
    self.streaming_config = None
    # 为新请求重置停止标志
    self._should_stop_streaming = False
    self._initialize_client()
    self.ten_env.log_debug("Vendor TTS client reset completed")
```

### 实现模板

基于Google TTS的完整实现模板：

```python
class VendorTTS:
    """SDK集成TTS客户端"""
    
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        # 初始化代码...
        
    def _initialize_client(self):
        """初始化SDK客户端"""
        # 初始化代码...
        
    def _parse_credentials(self):
        """解析认证信息"""
        # 认证解析代码...
        
    def _create_streaming_config(self) -> vendor_sdk.StreamingSynthesizeConfig:
        """创建流式配置"""
        # 配置创建代码...
        
    def _create_request_generator(self, text: str) -> Iterator[vendor_sdk.StreamingSynthesizeRequest]:
        """创建请求生成器"""
        # 请求生成代码...
        
    async def get_streaming(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """流式合成处理"""
        # 流式处理代码...
        
    async def get(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """生成TTS音频"""
        # 主处理方法...
        
    def clean(self):
        """清理资源"""
        # 清理代码...
        
    async def reset(self):
        """重置客户端"""
        # 重置代码...

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """TTS Extension实现"""
    
    def __init__(self, name: str) -> None:
        super().__init__(name)
        # 初始化代码...
        
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        # 初始化代码...
        
    async def request_tts(self, t: TTSTextInput) -> None:
        # TTS请求处理...
        
    def vendor(self) -> str:
        return "vendor_name"
        
    def synthesize_audio_sample_rate(self) -> int:
        return self.config.params.get("AudioConfig", {}).get("sample_rate_hertz", 24000)
```

### 最佳实践

1. **SDK版本管理**：使用稳定的SDK版本，避免频繁更新
2. **认证管理**：安全处理认证信息，支持多种认证方式
3. **配置映射**：将框架参数映射到SDK特定参数
4. **错误处理**：区分SDK错误和网络错误，采用不同处理策略
5. **资源管理**：及时清理SDK资源，避免内存泄漏
6. **重试策略**：实现智能重试机制，处理临时性错误
7. **性能优化**：使用流式处理，减少延迟
8. **兼容性**：确保SDK版本兼容性，处理API变更

## config.py规范

### 配置类结构

`config.py`文件负责管理TTS Extension的所有配置参数，包括认证信息、API参数、调试设置等。所有配置类都应继承自`pydantic.BaseModel`，提供类型验证和序列化功能。

### 基础配置类模板

```python
from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base import utils

class VendorTTSConfig(BaseModel):
    """TTS供应商配置类"""
    
    # 认证信息
    api_key: str = ""
    
    # 调试和日志设置
    dump: bool = False
    dump_path: str = "./"
    
    # 音频参数
    sample_rate: int = 16000
    channels: int = 1
    
    # 供应商特定参数
    params: Dict[str, Any] = Field(default_factory=dict)
    
    # 黑名单参数（将从params中删除）
    black_list_keys: List[str] = Field(default_factory=lambda: ["api_key"])
    
    def to_str(self, sensitive_handling: bool = False) -> str:
        """转换为字符串，支持敏感信息处理"""
        if not sensitive_handling:
            return f"{self}"
        
        config = self.copy(deep=True)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        return f"{config}"
    
    def update_params(self) -> None:
        """从params更新配置值"""
        for key, value in self.params.items():
            if hasattr(self, key):
                setattr(self, key, value)
        
        # 删除黑名单中的键
        for key in self.black_list_keys:
            if key in self.params:
                del self.params[key]
```

### 参数验证规范

#### 1. 必需参数验证

```python
def validate_params(self) -> None:
    """验证必需配置参数"""
    required_fields = ["api_key"]
    
    for field_name in required_fields:
        value = getattr(self, field_name)
        if not value or (isinstance(value, str) and value.strip() == ""):
            raise ValueError(f"Required field '{field_name}' is missing or empty")
```

#### 2. 参数类型验证

```python
from pydantic import field_validator

@field_validator('sample_rate')
@classmethod
def validate_sample_rate(cls, v):
    """验证采样率"""
    if v not in [8000, 16000, 22050, 24000, 44100, 48000]:
        raise ValueError(f"Unsupported sample rate: {v}")
    return v

@field_validator('channels')
@classmethod
def validate_channels(cls, v):
    """验证声道数"""
    if v not in [1, 2]:
        raise ValueError(f"Unsupported channel count: {v}")
    return v
```

#### 3. 供应商特定参数处理

```python
def update_params(self) -> None:
    """更新供应商特定参数"""
    # 提取API密钥
    if "api_key" in self.params:
        self.api_key = self.params["api_key"]
        del self.params["api_key"]
    
    # 处理音频设置
    if "audio_setting" in self.params:
        audio_setting = self.params["audio_setting"]
        if "sample_rate" in audio_setting:
            self.sample_rate = int(audio_setting["sample_rate"])
        if "channels" in audio_setting:
            self.channels = int(audio_setting["channels"])
    
    # 设置固定值
    if "audio_setting" not in self.params:
        self.params["audio_setting"] = {}
    self.params["audio_setting"]["format"] = "pcm"
    
    # 删除黑名单参数
    for key in self.black_list_keys:
        if key in self.params:
            del self.params[key]
```

### 敏感信息处理

#### 1. 加密处理

```python
def to_str(self, sensitive_handling: bool = False) -> str:
    """转换为字符串，支持敏感信息处理"""
    if not sensitive_handling:
        return f"{self}"
    
    config = self.copy(deep=True)
    
    # 加密敏感字段
    sensitive_fields = ["api_key", "credentials", "token"]
    for field in sensitive_fields:
        if hasattr(config, field):
            value = getattr(config, field)
            if value:
                setattr(config, field, utils.encrypt(value))
    
    return f"{config}"
```

#### 2. 环境变量支持

```python
def update_params(self) -> None:
    """支持环境变量"""
    import os
    
    # 从环境变量获取API密钥
    if "api_key" in self.params and self.params["api_key"].startswith("${env:"):
        env_var = self.params["api_key"][6:-1]  # 移除 ${env: 和 }
        self.api_key = os.getenv(env_var, "")
        del self.params["api_key"]
    
    # 其他参数处理...
```

### 配置验证最佳实践

1. **类型安全**：使用pydantic的类型验证，确保参数类型正确
2. **必需参数检查**：在`validate_params`方法中检查必需参数
3. **参数范围验证**：验证采样率、声道数等参数的有效范围
4. **敏感信息保护**：使用`to_str`方法的安全模式处理敏感信息
5. **环境变量支持**：支持从环境变量读取配置
6. **参数映射**：将通用参数映射到供应商特定参数
7. **默认值设置**：为所有参数提供合理的默认值
8. **错误信息清晰**：提供清晰的错误信息，便于调试

## {vendor}_tts.py规范

### 客户端类设计

`{vendor}_tts.py`文件是TTS Extension的核心实现文件，负责与供应商API的通信、音频数据处理和错误处理。根据不同的实现模式，客户端类的设计会有所不同。

### 基础客户端类结构

```python
import asyncio
import json
import time
from typing import AsyncIterator, Union, Callable, Awaitable
from ten_runtime import AsyncTenEnv
from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from .config import VendorTTSConfig

# 事件类型定义
EVENT_TTS_RESPONSE = 1
EVENT_TTS_REQUEST_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5

class VendorTTS:
    """TTS客户端基础类"""
    
    def __init__(
        self,
        config: VendorTTSConfig,
        ten_env: AsyncTenEnv,
        error_callback: Callable[[str, ModuleError], Awaitable[None]] = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.error_callback = error_callback
        
        # 状态管理
        self.send_text_in_connection = False
        self.cur_request_id = ""
        self._is_cancelled = False
        
        # 初始化客户端
        self._initialize_client()
    
    def _initialize_client(self):
        """初始化客户端 - 子类实现"""
        raise NotImplementedError("Subclasses must implement _initialize_client")
    
    async def get(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """生成TTS音频 - 子类实现"""
        raise NotImplementedError("Subclasses must implement get method")
    
    def cancel(self):
        """取消当前请求"""
        self._is_cancelled = True
        self.ten_env.log_debug("VendorTTS: cancel() called.")
    
    async def stop(self):
        """停止客户端"""
        if hasattr(self, 'client') and self.client:
            await self.client.aclose()
            self.client = None
```

### 事件系统设计

#### 1. 事件类型定义

```python
# 基础事件类型
EVENT_TTS_RESPONSE = 1          # 音频数据响应
EVENT_TTS_REQUEST_END = 2       # 请求结束
EVENT_TTS_ERROR = 3             # 一般错误
EVENT_TTS_INVALID_KEY_ERROR = 4 # 认证错误
EVENT_TTS_FLUSH = 5             # 刷新请求

# WebSocket特定事件
EVENT_TTSSentenceStart = 350    # 句子开始
EVENT_TTSSentenceEnd = 351      # 句子结束
EVENT_TTSTaskFinished = 353     # 任务完成
EVENT_TTS_TTFB_METRIC = 355     # TTFB指标

# HTTP特定事件
EVENT_TTS_END = 2               # HTTP请求结束
```

#### 2. 事件处理机制

```python
class VendorTTS:
    def _handle_response_event(self, data: bytes, event_type: int, ttfb_ms: int = None):
        """处理响应事件"""
        if event_type == EVENT_TTS_RESPONSE:
            self.ten_env.log_debug(f"Received audio data: {len(data)} bytes")
            return data, event_type, ttfb_ms
        elif event_type == EVENT_TTS_REQUEST_END:
            self.ten_env.log_debug("Request completed")
            return None, event_type, ttfb_ms
        elif event_type == EVENT_TTS_ERROR:
            self.ten_env.log_error(f"TTS Error: {data.decode('utf-8') if data else 'Unknown error'}")
            return data, event_type, ttfb_ms
        elif event_type == EVENT_TTS_INVALID_KEY_ERROR:
            self.ten_env.log_error(f"Authentication Error: {data.decode('utf-8') if data else 'Invalid credentials'}")
            return data, event_type, ttfb_ms
        elif event_type == EVENT_TTS_FLUSH:
            self.ten_env.log_debug("Flush request received")
            return None, event_type, ttfb_ms
```

### 资源管理规范

#### 1. 连接管理

```python
def cancel(self) -> None:
    """取消当前操作"""
    self._is_cancelled = True
    self._should_stop_streaming = True
    
    # 清理队列
    self._clear_queues()
    
    # 关闭连接
    if hasattr(self, 'ws') and self.ws:
        asyncio.create_task(self.ws.close())

def _clear_queues(self) -> None:
    """清理队列"""
    if hasattr(self, 'text_input_queue'):
        while not self.text_input_queue.empty():
            try:
                self.text_input_queue.get_nowait()
            except asyncio.QueueEmpty:
                break
    
    if hasattr(self, 'response_msgs') and self.response_msgs:
        while not self.response_msgs.empty():
            try:
                self.response_msgs.get_nowait()
            except asyncio.QueueEmpty:
                break

async def stop(self):
    """停止客户端"""
    self._is_cancelled = True
    
    # 停止WebSocket任务
    if hasattr(self, 'websocket_task') and self.websocket_task:
        self.websocket_task.cancel()
        try:
            await self.websocket_task
        except asyncio.CancelledError:
            pass
    
    # 关闭HTTP客户端
    if hasattr(self, 'client') and self.client:
        await self.client.aclose()
        self.client = None
```

### 客户端设计最佳实践

1. **状态管理**：使用清晰的状态标志管理连接和请求状态
2. **事件驱动**：使用事件系统解耦不同组件
3. **错误分类**：区分不同类型的错误，采用不同的处理策略
4. **资源清理**：及时清理资源，避免内存泄漏
5. **重试机制**：实现智能重试，处理临时性错误
6. **超时控制**：设置合理的超时时间
7. **日志记录**：记录关键操作和错误信息
8. **异步处理**：使用异步编程提高性能

## extension.py规范

### Extension类实现

`extension.py`文件是TTS Extension的业务逻辑控制层，继承自`AsyncTTS2BaseExtension`基类，负责处理TTS请求、管理音频数据流、处理生命周期事件等。

### 基础Extension类结构

```python
import asyncio
from datetime import datetime
import os
import traceback
from typing import Tuple

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .config import VendorTTSConfig
from .vendor_tts import VendorTTS, EVENT_TTS_RESPONSE, EVENT_TTS_REQUEST_END, EVENT_TTS_ERROR, EVENT_TTS_INVALID_KEY_ERROR
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """TTS Extension实现"""
    
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: VendorTTSConfig | None = None
        self.client: VendorTTS | None = None
        self.current_request_id: str | None = None
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.recorder_map: dict[str, PCMWriter] = {}
        self.last_complete_request_id: str | None = None
        self._flush_requested = False
```

### 生命周期管理

#### 1. 初始化阶段

```python
async def on_init(self, ten_env: AsyncTenEnv) -> None:
    """Extension初始化"""
    try:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")
        
        # 获取配置
        if self.config is None:
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = VendorTTSConfig.model_validate_json(config_json)
            self.config.update_params()
            
            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            
            # 验证必需参数
            if not self.config.api_key:
                raise ValueError("API key is required")
        
        # 创建错误回调
        async def error_callback(request_id: str, error: ModuleError):
            target_request_id = request_id if request_id else self.current_request_id or ""
            await self.send_tts_error(request_id=target_request_id, error=error)
            
            if error.code == ModuleErrorCode.FATAL_ERROR:
                ten_env.log_error(f"Fatal error occurred: {error.message}")
                await self.client.stop()
                await self.on_stop(self.ten_env)
        
        # 创建客户端
        self.client = VendorTTS(
            config=self.config,
            ten_env=ten_env,
            error_callback=error_callback,
        )
        
    except Exception as e:
        ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
        await self.send_tts_error(
            request_id="",
            error=ModuleError(
                message=str(e),
                module=ModuleType.TTS,
                code=ModuleErrorCode.FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
```

#### 2. 启动阶段

```python
async def on_start(self, ten_env: AsyncTenEnv) -> None:
    """Extension启动"""
    await super().on_start(ten_env)
    ten_env.log_debug("on_start")
    
    # 预热客户端连接（可选）
    if hasattr(self.client, 'start'):
        await self.client.start()
```

#### 3. 停止阶段

```python
async def on_stop(self, ten_env: AsyncTenEnv) -> None:
    """Extension停止"""
    ten_env.log_debug("VendorTTS extension on_stop started")
    
    # 停止客户端
    if self.client:
        try:
            await self.client.stop()
        except Exception as e:
            ten_env.log_error(f"Error stopping VendorTTS client: {e}")
        finally:
            self.client = None
    
    # 清理PCMWriter
    for request_id, recorder in self.recorder_map.items():
        try:
            await recorder.flush()
            ten_env.log_debug(f"Flushed PCMWriter for request_id: {request_id}")
        except Exception as e:
            ten_env.log_error(f"Error flushing PCMWriter for request_id {request_id}: {e}")
    
    # 清理状态
    self.recorder_map.clear()
    self.last_complete_request_id = None
    
    await super().on_stop(ten_env)
    ten_env.log_debug("on_stop")
```

#### 4. 销毁阶段

```python
async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
    """Extension销毁"""
    await super().on_deinit(ten_env)
    ten_env.log_debug("on_deinit")
```

### TTS请求处理

#### 1. 核心请求处理

```python
async def request_tts(self, t: TTSTextInput) -> None:
    """处理TTS请求"""
    try:
        if not self.client or not self.config:
            raise RuntimeError("Extension is not initialized properly.")
        
        # 检查请求ID是否已完成
        if self.last_complete_request_id == t.request_id:
            self.ten_env.log_debug(f"Request ID {t.request_id} has already been completed, ignoring")
            return
        
        # 处理新请求ID
        if t.request_id != self.current_request_id:
            self.current_request_id = t.request_id
            self._reset_request_state()
            self._flush_requested = False
            
            # 重置连接（如果需要）
            if self.client and self.client.send_text_in_connection:
                self.ten_env.log_debug("Resetting client since request id changed")
                await self.handle_completed_request(TTSAudioEndReason.INTERRUPTED)
                await self.client.stop()
                await self.client.start()
            
            # 创建PCMWriter（如果启用dump）
            await self._setup_pcm_writer(t.request_id)
        
        # 处理文本
        if t.text.strip() != "":
            await self._process_text_request(t)
        
        # 处理请求结束
        if t.text_input_end:
            self.current_request_finished = True
            await self.handle_completed_request(TTSAudioEndReason.REQUEST_END)
            
            # 重置连接
            if self.client and self.client.send_text_in_connection:
                await self.client.stop()
                await self.client.start()
    
    except Exception as e:
        self.ten_env.log_error(f"Error in request_tts: {traceback.format_exc()}")
        await self.send_tts_error(
            request_id=self.current_request_id or t.request_id,
            error=ModuleError(
                message=str(e),
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
```

#### 2. 文本处理

```python
async def _process_text_request(self, t: TTSTextInput) -> None:
    """处理文本请求"""
    self.ten_env.log_debug(
        f"send_text_to_tts_server: {t.text} of request_id: {t.request_id}",
        category=LOG_CATEGORY_VENDOR,
    )
    
    # 获取音频生成器
    audio_generator = self.client.get(t.text, t.request_id)
    
    try:
        async for audio_chunk, event, ttfb_ms in audio_generator:
            # 检查是否被刷新
            if self._flush_requested:
                self.ten_env.log_debug("Flush requested, stopping audio processing")
                break
            
            if event == EVENT_TTS_RESPONSE and audio_chunk:
                await self._handle_audio_chunk(audio_chunk, ttfb_ms)
            elif event == EVENT_TTS_REQUEST_END:
                break
            elif event == EVENT_TTS_INVALID_KEY_ERROR:
                await self._handle_authentication_error(audio_chunk)
                return
            elif event == EVENT_TTS_ERROR:
                error_msg = audio_chunk.decode("utf-8") if audio_chunk else "Unknown error"
                raise RuntimeError(error_msg)
    
    except Exception as e:
        self.ten_env.log_error(f"Error in audio processing: {traceback.format_exc()}")
        await self.send_tts_error(
            request_id=self.current_request_id or t.request_id,
            error=ModuleError(
                message=str(e),
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
    finally:
        # 确保生成器被正确关闭
        try:
            await audio_generator.aclose()
        except Exception as e:
            self.ten_env.log_error(f"Error closing audio generator: {e}")
```

#### 3. 音频数据处理

```python
async def _handle_audio_chunk(self, audio_chunk: bytes, ttfb_ms: int = None) -> None:
    """处理音频数据块"""
    self.total_audio_bytes += len(audio_chunk)
    
    # 计算音频时长
    duration_ms = self._calculate_audio_duration_ms()
    
    self.ten_env.log_debug(
        f"receive_audio: duration: {duration_ms} of request id: {self.current_request_id}",
        category=LOG_CATEGORY_VENDOR,
    )
    
    # 发送音频开始事件（首次）
    if self.sent_ts is None and self.current_request_id:
        self.sent_ts = datetime.now()
        await self.send_tts_audio_start(request_id=self.current_request_id)
        
        # 发送TTFB指标
        if ttfb_ms is not None:
            extra_metadata = self._get_extra_metadata()
            await self.send_tts_ttfb_metrics(
                request_id=self.current_request_id,
                ttfb_ms=ttfb_ms,
                extra_metadata=extra_metadata,
            )
    
    # 写入PCM文件（如果启用dump）
    if self.config.dump and self.current_request_id in self.recorder_map:
        await self.recorder_map[self.current_request_id].write(audio_chunk)
    
    # 发送音频数据
    await self.send_tts_audio_data(audio_chunk)

def _calculate_audio_duration_ms(self) -> int:
    """计算音频时长（毫秒）"""
    if self.config is None:
        return 0
    
    bytes_per_sample = 2  # 16-bit PCM
    channels = 1  # Mono
    duration_sec = self.total_audio_bytes / (
        self.synthesize_audio_sample_rate() * bytes_per_sample * channels
    )
    return int(duration_sec * 1000)

def _get_extra_metadata(self) -> dict:
    """获取额外元数据"""
    return {
        "voice_id": self.config.params.get("voice_id", ""),
        "model_id": self.config.params.get("model_id", ""),
    }
```

### 请求完成处理

```python
async def handle_completed_request(self, reason: TTSAudioEndReason):
    """处理请求完成"""
    # 更新完成请求ID
    if self.last_complete_request_id == self.current_request_id:
        self.ten_env.log_debug(f"{self.current_request_id} was completed, skip.")
        return
    
    self.last_complete_request_id = self.current_request_id
    self.ten_env.log_debug(f"update last_complete_request_id to: {self.current_request_id}")
    
    # 刷新PCMWriter
    if self.config.dump and self.current_request_id in self.recorder_map:
        try:
            await self.recorder_map[self.current_request_id].flush()
            self.ten_env.log_debug(f"Flushed PCMWriter for request_id: {self.current_request_id}")
        except Exception as e:
            self.ten_env.log_error(f"Error flushing PCMWriter: {e}")
    
    # 发送音频结束事件
    request_event_interval = 0
    if self.sent_ts is not None:
        request_event_interval = int(
            (datetime.now() - self.sent_ts).total_seconds() * 1000
        )
    
    duration_ms = self._calculate_audio_duration_ms()
    
    await self.send_tts_audio_end(
        request_id=self.current_request_id or "",
        request_event_interval_ms=request_event_interval,
        request_total_audio_duration_ms=duration_ms,
        reason=reason,
    )
    
    self.ten_env.log_debug(
        f"Sent tts_audio_end with {reason.name} reason for request_id: {self.current_request_id}"
    )
    
    # 重置状态
    self.sent_ts = None
    self.total_audio_bytes = 0
```

### flush处理

```python
async def cancel_tts(self) -> None:
    """取消TTS请求"""
    self._flush_requested = True
    
    try:
        if self.client is not None:
            self.ten_env.log_info("Flushing VendorTTS client")
            await self.client.stop()
            await self.client.start()
        else:
            self.ten_env.log_warn("Client is not initialized, skipping reset")
    
    except Exception as e:
        self.ten_env.log_error(f"Error in handle_flush: {e}")
        await self.send_tts_error(
            request_id=self.current_request_id,
            error=ModuleError(
                message=str(e),
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
    
    await self.handle_completed_request(TTSAudioEndReason.INTERRUPTED)
```

### PCM文件管理

```python
async def _setup_pcm_writer(self, request_id: str) -> None:
    """设置PCM文件写入器"""
    if not self.config.dump:
        return
    
    # 清理旧的PCMWriter
    old_request_ids = [rid for rid in self.recorder_map.keys() if rid != request_id]
    for old_rid in old_request_ids:
        try:
            await self.recorder_map[old_rid].flush()
            del self.recorder_map[old_rid]
            self.ten_env.log_debug(f"Cleaned up old PCMWriter for request_id: {old_rid}")
        except Exception as e:
            self.ten_env.log_error(f"Error cleaning up PCMWriter for request_id {old_rid}: {e}")
    
    # 创建新的PCMWriter
    if request_id not in self.recorder_map:
        dump_file_path = os.path.join(
            self.config.dump_path,
            f"vendor_dump_{request_id}.pcm",
        )
        self.recorder_map[request_id] = PCMWriter(dump_file_path)
        self.ten_env.log_info(f"Created PCMWriter for request_id: {request_id}, file: {dump_file_path}")
```

### 必需方法实现

```python
def vendor(self) -> str:
    """返回供应商名称"""
    return "vendor_name"

def synthesize_audio_sample_rate(self) -> int:
    """返回音频采样率"""
    if self.config and self.config.params:
        audio_params = self.config.params.get("AudioConfig", {})
        if audio_params.get("sample_rate_hertz"):
            return audio_params.get("sample_rate_hertz")
    return self.config.sample_rate if self.config else 16000

def synthesize_audio_channels(self) -> int:
    """返回音频声道数"""
    return 1

def synthesize_audio_sample_width(self) -> int:
    """返回音频采样宽度"""
    return 2
```

### Extension类设计最佳实践

1. **状态管理**：清晰管理请求状态和生命周期
2. **错误处理**：统一的错误处理和报告机制
3. **资源清理**：及时清理PCMWriter和客户端资源
4. **指标上报**：准确上报TTFB和音频时长指标
5. **请求去重**：避免重复处理已完成的请求
6. **连接管理**：合理管理客户端连接的生命周期
7. **日志记录**：记录关键操作和状态变化
8. **异常安全**：确保异常情况下的资源清理

## 测试规范

### 测试文件结构

TTS Extension的测试目录应包含完整的测试套件，确保代码质量和功能正确性。

```
tests/
├── __init__.py                    # 测试包初始化
├── conftest.py                   # pytest配置和fixtures
├── test_basic.py                 # 基础功能测试
├── test_error_msg.py             # 错误处理测试
├── test_params.py                # 参数配置测试
├── test_robustness.py            # 健壮性测试
├── test_metrics.py               # 指标测试
└── configs/                      # 测试配置文件
    ├── test_config.json          # 测试配置
    ├── invalid_config.json       # 无效配置测试
    └── mock_config.json          # Mock测试配置
```

### conftest.py配置

```python
import json
import threading
from typing_extensions import override
import pytest
from ten_runtime import App, TenEnv

class FakeApp(App):
    """测试用假应用"""
    
    def __init__(self):
        super().__init__()
        self.event: threading.Event | None = None
    
    @override
    def on_init(self, ten_env: TenEnv) -> None:
        assert self.event
        self.event.set()
        ten_env.on_init_done()
    
    @override
    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.init_property_from_json(
            json.dumps({
                "ten": {
                    "log": {
                        "handlers": [
                            {
                                "matchers": [{"level": "debug"}],
                                "formatter": {
                                    "type": "plain",
                                    "colored": True,
                                },
                            }
                        ]
                    }
                }
            })
        )
        ten_env.on_configure_done()

@pytest.fixture
def fake_app():
    """创建假应用"""
    return FakeApp()

@pytest.fixture
def test_config():
    """测试配置"""
    return {
        "api_key": "test_api_key",
        "dump": True,
        "dump_path": "./test_dump/",
        "sample_rate": 16000,
        "params": {
            "voice_id": "test_voice",
            "model_id": "test_model",
        }
    }

@pytest.fixture
def invalid_config():
    """无效配置"""
    return {
        "api_key": "",  # 空API密钥
        "sample_rate": 99999,  # 无效采样率
    }

@pytest.fixture
def mock_config():
    """Mock测试配置"""
    return {
        "api_key": "mock_api_key",
        "dump": False,
        "sample_rate": 16000,
        "params": {
            "voice_id": "mock_voice",
            "mock_mode": True,
        }
    }
```

### 基础功能测试

```python
import sys
from pathlib import Path
import json
import tempfile
import os
import asyncio
import filecmp
import shutil
from unittest.mock import patch, AsyncMock

from ten_runtime import ExtensionTester, TenEnvTester, Cmd, CmdResult, StatusCode, Data
from ten_ai_base.struct import TTSTextInput, TTSFlush
from ten_ai_base.message import ModuleVendorException, ModuleErrorVendorInfo

class ExtensionTesterDump(ExtensionTester):
    """支持dump的Extension测试器"""
    
    def __init__(self):
        super().__init__()
        self.dump_dir = "./dump/"
        self.test_dump_file_path = os.path.join(self.dump_dir, "test_manual_dump.pcm")
        self.audio_end_received = False
        self.received_audio_chunks = []
        self.error_received = False
        self.ttfb_metrics_received = False
    
    async def on_audio_frame(self, audio_frame):
        """处理音频帧"""
        self.received_audio_chunks.append(audio_frame.get_data())
    
    async def on_data(self, data: Data):
        """处理数据"""
        data_name = data.get_name()
        if data_name == "tts_audio_end":
            self.audio_end_received = True
        elif data_name == "metrics":
            self.ttfb_metrics_received = True
        elif data_name == "error":
            self.error_received = True

class TestBasicFunctionality:
    """基础功能测试"""
    
    @pytest.mark.asyncio
    async def test_extension_initialization(self, fake_app, test_config):
        """测试Extension初始化"""
        with tempfile.TemporaryDirectory() as temp_dir:
            # 创建测试配置
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            # 创建测试器
            tester = ExtensionTesterDump()
            
            # 测试初始化
            await tester.start(fake_app, config_path)
            
            # 验证初始化成功
            assert tester.extension is not None
            assert tester.extension.config is not None
            assert tester.extension.config.api_key == "test_api_key"
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_tts_request_processing(self, fake_app, test_config):
        """测试TTS请求处理"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 创建TTS请求
            tts_input = TTSTextInput(
                text="Hello, world!",
                request_id="test_request_1",
                text_input_end=True
            )
            
            # 发送请求
            await tester.send_tts_text_input(tts_input)
            
            # 等待处理完成
            await asyncio.sleep(2)
            
            # 验证结果
            assert len(tester.received_audio_chunks) > 0
            assert tester.audio_end_received
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_multiple_requests(self, fake_app, test_config):
        """测试多个请求处理"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 发送多个请求
            for i in range(3):
                tts_input = TTSTextInput(
                    text=f"Request {i+1}",
                    request_id=f"test_request_{i+1}",
                    text_input_end=True
                )
                await tester.send_tts_text_input(tts_input)
                await asyncio.sleep(1)
            
            # 验证结果
            assert len(tester.received_audio_chunks) >= 3
            
            await tester.stop()
```

### 错误处理测试

```python
class TestErrorHandling:
    """错误处理测试"""
    
    @pytest.mark.asyncio
    async def test_invalid_api_key(self, fake_app, invalid_config):
        """测试无效API密钥"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(invalid_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 发送请求
            tts_input = TTSTextInput(
                text="Test text",
                request_id="test_request",
                text_input_end=True
            )
            await tester.send_tts_text_input(tts_input)
            
            # 等待错误处理
            await asyncio.sleep(2)
            
            # 验证错误被正确处理
            assert tester.error_received
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_network_error_handling(self, fake_app, test_config):
        """测试网络错误处理"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            
            # Mock网络错误
            with patch('vendor_tts.VendorTTS.get') as mock_get:
                mock_get.side_effect = Exception("Network error")
                
                await tester.start(fake_app, config_path)
                
                tts_input = TTSTextInput(
                    text="Test text",
                    request_id="test_request",
                    text_input_end=True
                )
                await tester.send_tts_text_input(tts_input)
                
                await asyncio.sleep(2)
                
                # 验证错误处理
                assert tester.error_received
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_flush_handling(self, fake_app, test_config):
        """测试刷新处理"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 发送请求
            tts_input = TTSTextInput(
                text="Long text that will be interrupted",
                request_id="test_request",
                text_input_end=False
            )
            await tester.send_tts_text_input(tts_input)
            
            # 等待一段时间后发送刷新
            await asyncio.sleep(1)
            
            flush_input = TTSFlush(
                flush_id="test_flush",
                metadata={}
            )
            await tester.send_tts_flush(flush_input)
            
            await asyncio.sleep(1)
            
            # 验证刷新处理
            assert tester.audio_end_received
            
            await tester.stop()
```

### 参数配置测试

```python
class TestParameterConfiguration:
    """参数配置测试"""
    
    @pytest.mark.asyncio
    async def test_sample_rate_configuration(self, fake_app):
        """测试采样率配置"""
        test_configs = [
            {"sample_rate": 8000},
            {"sample_rate": 16000},
            {"sample_rate": 24000},
            {"sample_rate": 48000},
        ]
        
        for config in test_configs:
            with tempfile.TemporaryDirectory() as temp_dir:
                config_path = os.path.join(temp_dir, "property.json")
                with open(config_path, 'w') as f:
                    json.dump({
                        "api_key": "test_key",
                        **config
                    }, f)
                
                tester = ExtensionTesterDump()
                await tester.start(fake_app, config_path)
                
                # 验证采样率设置
                assert tester.extension.synthesize_audio_sample_rate() == config["sample_rate"]
                
                await tester.stop()
    
    @pytest.mark.asyncio
    async def test_voice_parameters(self, fake_app):
        """测试语音参数"""
        test_config = {
            "api_key": "test_key",
            "params": {
                "voice_id": "test_voice_123",
                "model_id": "test_model_456",
                "speaking_rate": 1.2,
                "pitch": 0.5,
            }
        }
        
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 验证参数设置
            extension = tester.extension
            assert extension.config.params["voice_id"] == "test_voice_123"
            assert extension.config.params["model_id"] == "test_model_456"
            assert extension.config.params["speaking_rate"] == 1.2
            assert extension.config.params["pitch"] == 0.5
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_dump_configuration(self, fake_app):
        """测试dump配置"""
        test_config = {
            "api_key": "test_key",
            "dump": True,
            "dump_path": "./test_dump/"
        }
        
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 验证dump配置
            extension = tester.extension
            assert extension.config.dump == True
            assert extension.config.dump_path == "./test_dump/"
            
            await tester.stop()
```

### 健壮性测试

```python
class TestRobustness:
    """健壮性测试"""
    
    @pytest.mark.asyncio
    async def test_concurrent_requests(self, fake_app, test_config):
        """测试并发请求"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 并发发送多个请求
            tasks = []
            for i in range(5):
                tts_input = TTSTextInput(
                    text=f"Concurrent request {i}",
                    request_id=f"concurrent_request_{i}",
                    text_input_end=True
                )
                task = asyncio.create_task(tester.send_tts_text_input(tts_input))
                tasks.append(task)
            
            # 等待所有请求完成
            await asyncio.gather(*tasks)
            await asyncio.sleep(3)
            
            # 验证结果
            assert len(tester.received_audio_chunks) >= 5
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_large_text_handling(self, fake_app, test_config):
        """测试大文本处理"""
        large_text = "This is a very long text. " * 100  # 创建大文本
        
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            tts_input = TTSTextInput(
                text=large_text,
                request_id="large_text_request",
                text_input_end=True
            )
            await tester.send_tts_text_input(tts_input)
            
            await asyncio.sleep(5)  # 等待更长时间
            
            # 验证大文本处理
            assert len(tester.received_audio_chunks) > 0
            
            await tester.stop()
    
    @pytest.mark.asyncio
    async def test_special_characters(self, fake_app, test_config):
        """测试特殊字符处理"""
        special_texts = [
            "Hello, 世界!",  # 中英文混合
            "Special chars: @#$%^&*()",  # 特殊符号
            "Emoji: 😀😁😂🤣",  # 表情符号
            "Unicode: αβγδε",  # Unicode字符
        ]
        
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            for i, text in enumerate(special_texts):
                tts_input = TTSTextInput(
                    text=text,
                    request_id=f"special_text_{i}",
                    text_input_end=True
                )
                await tester.send_tts_text_input(tts_input)
                await asyncio.sleep(1)
            
            # 验证特殊字符处理
            assert len(tester.received_audio_chunks) >= len(special_texts)
            
            await tester.stop()
```

### Mock测试

```python
class TestMockFunctionality:
    """Mock测试"""
    
    @pytest.mark.asyncio
    async def test_mock_client(self, fake_app, mock_config):
        """测试Mock客户端"""
        with patch('vendor_tts.VendorTTS') as mock_client_class:
            # 创建Mock客户端
            mock_client = AsyncMock()
            mock_client.get.return_value = self._mock_audio_generator()
            mock_client_class.return_value = mock_client
            
            with tempfile.TemporaryDirectory() as temp_dir:
                config_path = os.path.join(temp_dir, "property.json")
                with open(config_path, 'w') as f:
                    json.dump(mock_config, f)
                
                tester = ExtensionTesterDump()
                await tester.start(fake_app, config_path)
                
                tts_input = TTSTextInput(
                    text="Mock test",
                    request_id="mock_request",
                    text_input_end=True
                )
                await tester.send_tts_text_input(tts_input)
                
                await asyncio.sleep(1)
                
                # 验证Mock调用
                mock_client.get.assert_called_once()
                
                await tester.stop()
    
    async def _mock_audio_generator(self):
        """Mock音频生成器"""
        # 模拟音频数据
        mock_audio = b"mock_audio_data"
        yield mock_audio, 1, 100  # audio_data, event, ttfb_ms
        yield None, 2, None  # None, end_event, None
```

### 测试最佳实践

1. **测试覆盖**：确保所有主要功能都有测试覆盖
2. **Mock使用**：合理使用Mock避免依赖外部服务
3. **异步测试**：正确处理异步测试和等待
4. **资源清理**：确保测试后正确清理资源
5. **错误场景**：测试各种错误和异常情况
6. **边界测试**：测试边界条件和极限情况
7. **并发测试**：测试并发请求处理能力
8. **配置测试**：测试各种配置参数组合

## 配置文件规范

### manifest.json配置

`manifest.json`文件定义了TTS Extension的元数据、依赖关系和API接口，是Extension的核心配置文件。

#### 基础结构

```json
{
  "name": "vendor_tts_python",
  "version": "1.0.0",
  "description": "Vendor TTS Extension for TEN Framework",
  "author": "Your Name",
  "license": "MIT",
  "dependencies": {
    "ten_ai_base": ">=1.0.0",
    "ten_runtime": ">=1.0.0"
  },
  "package": {
    "include": [
      "vendor_tts.py",
      "config.py",
      "extension.py",
      "requirements.txt"
    ],
    "exclude": [
      "tests/",
      "*.pyc",
      "__pycache__/"
    ]
  },
  "api": {
    "name": "vendor_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "description": "TTS configuration parameters",
        "properties": {
          "key": {
            "type": "string",
            "description": "API key for authentication",
            "minLength": 1
          },
          "model_id": {
            "type": "string",
            "description": "TTS model identifier",
            "default": "default_model"
          },
          "voice_id": {
            "type": "string",
            "description": "Voice identifier",
            "default": "default_voice"
          },
          "request_timeout_seconds": {
            "type": "integer",
            "description": "Request timeout in seconds",
            "minimum": 1,
            "maximum": 300,
            "default": 30
          },
          "similarity_boost": {
            "type": "number",
            "description": "Voice similarity boost",
            "minimum": 0.0,
            "maximum": 1.0,
            "default": 0.5
          },
          "speaker_boost": {
            "type": "boolean",
            "description": "Enable speaker boost",
            "default": true
          },
          "stability": {
            "type": "number",
            "description": "Voice stability",
            "minimum": 0.0,
            "maximum": 1.0,
            "default": 0.5
          },
          "style": {
            "type": "string",
            "description": "Voice style",
            "enum": ["default", "narrative", "conversational"],
            "default": "default"
          },
          "optimize_streaming_latency": {
            "type": "integer",
            "description": "Streaming latency optimization level",
            "minimum": 0,
            "maximum": 4,
            "default": 2
          },
          "dump": {
            "type": "boolean",
            "description": "Enable audio dump",
            "default": false
          },
          "dump_path": {
            "type": "string",
            "description": "Audio dump file path",
            "default": "./dump/"
          }
        },
        "required": ["key"]
      }
    }
  }
}
```

#### WebSocket模式配置

```json
{
  "name": "vendor_websocket_tts_python",
  "version": "1.0.0",
  "description": "Vendor WebSocket TTS Extension",
  "api": {
    "name": "vendor_websocket_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "properties": {
          "key": {
            "type": "string",
            "description": "WebSocket API key"
          },
          "voice_id": {
            "type": "string",
            "description": "Voice identifier"
          },
          "model_id": {
            "type": "string",
            "description": "Model identifier"
          },
          "websocket_url": {
            "type": "string",
            "description": "WebSocket endpoint URL",
            "default": "wss://api.vendor.com/v1/text-to-speech"
          },
          "reconnect_interval": {
            "type": "integer",
            "description": "Reconnection interval in seconds",
            "minimum": 1,
            "maximum": 60,
            "default": 5
          },
          "max_reconnect_attempts": {
            "type": "integer",
            "description": "Maximum reconnection attempts",
            "minimum": 1,
            "maximum": 10,
            "default": 3
          },
          "heartbeat_interval": {
            "type": "integer",
            "description": "Heartbeat interval in seconds",
            "minimum": 10,
            "maximum": 300,
            "default": 30
          }
        },
        "required": ["key", "voice_id"]
      }
    }
  }
}
```

#### HTTP模式配置

```json
{
  "name": "vendor_http_tts_python",
  "version": "1.0.0",
  "description": "Vendor HTTP TTS Extension",
  "api": {
    "name": "vendor_http_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "properties": {
          "key": {
            "type": "string",
            "description": "HTTP API key"
          },
          "model_id": {
            "type": "string",
            "description": "Model identifier"
          },
          "voice_id": {
            "type": "string",
            "description": "Voice identifier"
          },
          "http_url": {
            "type": "string",
            "description": "HTTP endpoint URL",
            "default": "https://api.vendor.com/v1/tts"
          },
          "timeout": {
            "type": "integer",
            "description": "Request timeout in seconds",
            "minimum": 5,
            "maximum": 300,
            "default": 30
          },
          "max_retries": {
            "type": "integer",
            "description": "Maximum retry attempts",
            "minimum": 0,
            "maximum": 5,
            "default": 3
          },
          "retry_delay": {
            "type": "number",
            "description": "Retry delay in seconds",
            "minimum": 0.1,
            "maximum": 10.0,
            "default": 1.0
          },
          "chunk_size": {
            "type": "integer",
            "description": "Response chunk size",
            "minimum": 1024,
            "maximum": 65536,
            "default": 4096
          }
        },
        "required": ["key", "model_id"]
      }
    }
  }
}
```

#### SDK模式配置

```json
{
  "name": "vendor_sdk_tts_python",
  "version": "1.0.0",
  "description": "Vendor SDK TTS Extension",
  "api": {
    "name": "vendor_sdk_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "properties": {
          "credentials": {
            "type": "string",
            "description": "SDK credentials JSON string"
          },
          "project_id": {
            "type": "string",
            "description": "Project identifier"
          },
          "location": {
            "type": "string",
            "description": "Service location",
            "default": "us-central1"
          },
          "voice_name": {
            "type": "string",
            "description": "Voice name",
            "default": "en-US-Wavenet-D"
          },
          "language_code": {
            "type": "string",
            "description": "Language code",
            "default": "en-US"
          },
          "audio_encoding": {
            "type": "string",
            "description": "Audio encoding format",
            "enum": ["LINEAR16", "MP3", "OGG_OPUS"],
            "default": "LINEAR16"
          },
          "speaking_rate": {
            "type": "number",
            "description": "Speaking rate",
            "minimum": 0.25,
            "maximum": 4.0,
            "default": 1.0
          },
          "pitch": {
            "type": "number",
            "description": "Voice pitch",
            "minimum": -20.0,
            "maximum": 20.0,
            "default": 0.0
          },
          "volume_gain_db": {
            "type": "number",
            "description": "Volume gain in dB",
            "minimum": -96.0,
            "maximum": 16.0,
            "default": 0.0
          }
        },
        "required": ["credentials", "project_id"]
      }
    }
  }
}
```

### property.json配置

`property.json`文件提供Extension的默认配置值，支持环境变量和动态配置。

#### 基础结构

```json
{
  "dump": false,
  "dump_path": "./dump/",
  "key": "${VENDOR_TTS_API_KEY}",
  "model_id": "default_model",
  "voice_id": "default_voice",
  "output_format": "pcm_16000"
}
```

#### WebSocket模式默认配置

```json
{
  "dump": false,
  "dump_path": "./dump/",
  "key": "${VENDOR_WS_API_KEY}",
  "voice_id": "default_voice",
  "model_id": "default_model",
  "websocket_url": "wss://api.vendor.com/v1/text-to-speech",
  "reconnect_interval": 5,
  "max_reconnect_attempts": 3,
  "heartbeat_interval": 30,
  "similarity_boost": 0.5,
  "speaker_boost": true,
  "stability": 0.5,
  "style": "default",
  "optimize_streaming_latency": 2,
  "request_timeout_seconds": 30
}
```

#### HTTP模式默认配置

```json
{
  "dump": false,
  "dump_path": "./dump/",
  "key": "${VENDOR_HTTP_API_KEY}",
  "model_id": "default_model",
  "voice_id": "default_voice",
  "http_url": "https://api.vendor.com/v1/tts",
  "timeout": 30,
  "max_retries": 3,
  "retry_delay": 1.0,
  "chunk_size": 4096,
  "speaking_rate": 1.0,
  "pitch": 0.0,
  "volume_gain_db": 0.0
}
```

#### SDK模式默认配置

```json
{
  "dump": false,
  "dump_path": "./dump/",
  "credentials": "${VENDOR_SDK_CREDENTIALS}",
  "project_id": "${VENDOR_PROJECT_ID}",
  "location": "us-central1",
  "voice_name": "en-US-Wavenet-D",
  "language_code": "en-US",
  "audio_encoding": "LINEAR16",
  "speaking_rate": 1.0,
  "pitch": 0.0,
  "volume_gain_db": 0.0,
  "sample_rate_hertz": 16000
}
```

### 环境变量支持

#### 环境变量命名规范

```bash
# API密钥
VENDOR_TTS_API_KEY=your_api_key_here
VENDOR_WS_API_KEY=your_websocket_key_here
VENDOR_HTTP_API_KEY=your_http_key_here

# SDK认证信息
VENDOR_SDK_CREDENTIALS='{"type":"service_account","project_id":"..."}'
VENDOR_PROJECT_ID=your_project_id

# 可选配置
VENDOR_DUMP_ENABLED=true
VENDOR_DUMP_PATH=/path/to/dump/
VENDOR_TIMEOUT_SECONDS=30
VENDOR_MAX_RETRIES=3
```

#### 环境变量处理

```python
import os
from typing import Any, Dict

def load_from_env(key: str, default: Any = None) -> Any:
    """从环境变量加载配置"""
    value = os.getenv(key)
    if value is None:
        return default
    
    # 尝试解析JSON
    if value.startswith('{') or value.startswith('['):
        try:
            import json
            return json.loads(value)
        except json.JSONDecodeError:
            pass
    
    # 尝试解析布尔值
    if value.lower() in ('true', 'false'):
        return value.lower() == 'true'
    
    # 尝试解析数字
    try:
        if '.' in value:
            return float(value)
        else:
            return int(value)
    except ValueError:
        pass
    
    return value

def resolve_config(config: Dict[str, Any]) -> Dict[str, Any]:
    """解析配置中的环境变量"""
    resolved = {}
    
    for key, value in config.items():
        if isinstance(value, str) and value.startswith('${') and value.endswith('}'):
            env_key = value[2:-1]
            resolved[key] = load_from_env(env_key)
        else:
            resolved[key] = value
    
    return resolved
```

### 配置验证规范

#### JSON Schema验证

```python
import jsonschema
from typing import Dict, Any

def validate_config(config: Dict[str, Any], schema: Dict[str, Any]) -> None:
    """验证配置是否符合schema"""
    try:
        jsonschema.validate(config, schema)
    except jsonschema.ValidationError as e:
        raise ValueError(f"Configuration validation error: {e.message}")

# 基础配置schema
BASE_CONFIG_SCHEMA = {
    "type": "object",
    "properties": {
        "key": {"type": "string", "minLength": 1},
        "dump": {"type": "boolean"},
        "dump_path": {"type": "string"},
        "sample_rate": {"type": "integer", "minimum": 8000, "maximum": 48000}
    },
    "required": ["key"]
}

# WebSocket配置schema
WEBSOCKET_CONFIG_SCHEMA = {
    "type": "object",
    "properties": {
        **BASE_CONFIG_SCHEMA["properties"],
        "websocket_url": {"type": "string", "format": "uri"},
        "reconnect_interval": {"type": "integer", "minimum": 1, "maximum": 60},
        "max_reconnect_attempts": {"type": "integer", "minimum": 1, "maximum": 10},
        "heartbeat_interval": {"type": "integer", "minimum": 10, "maximum": 300}
    },
    "required": ["key", "websocket_url"]
}
```

### 配置文件最佳实践

1. **版本管理**：使用语义化版本号管理Extension版本
2. **依赖声明**：明确声明所有依赖包和版本要求
3. **参数验证**：使用JSON Schema验证配置参数
4. **环境变量**：支持环境变量配置敏感信息
5. **默认值**：为所有参数提供合理的默认值
6. **文档说明**：为每个参数提供清晰的描述
7. **类型安全**：使用正确的数据类型和约束
8. **向后兼容**：保持配置的向后兼容性

## 开发流程和最佳实践

### 开发步骤

#### 1. 项目初始化

```bash
# 创建项目目录
mkdir vendor_tts_python
cd vendor_tts_python

# 创建基础目录结构
mkdir tests
mkdir tests/configs

# 创建基础文件
touch __init__.py
touch config.py
touch vendor_tts.py
touch extension.py
touch requirements.txt
touch manifest.json
touch property.json
touch README.md

# 创建测试文件
touch tests/__init__.py
touch tests/conftest.py
touch tests/test_basic.py
touch tests/test_error_msg.py
touch tests/test_params.py
touch tests/test_robustness.py
```

#### 2. 依赖管理

```bash
# 创建虚拟环境
python -m venv venv
source venv/bin/activate  # Linux/Mac
# 或
venv\Scripts\activate  # Windows

# 安装基础依赖
pip install pydantic>=2.0.0
pip install pytest>=7.0.0
pip install pytest-asyncio>=0.21.0

# 根据实现模式安装特定依赖
# WebSocket模式
pip install websockets>=15.0

# HTTP模式
pip install httpx>=0.24.0

# SDK模式
pip install google-cloud-texttospeech>=2.0.0
# 或其他供应商SDK
```

#### 3. 配置开发

```python
# 1. 首先实现config.py
from pydantic import BaseModel, Field
from typing import Dict, Any, Optional

class VendorTTSConfig(BaseModel):
    """TTS配置类"""
    api_key: str = Field(..., description="API密钥")
    dump: bool = Field(default=False, description="是否启用dump")
    dump_path: str = Field(default="./dump/", description="dump路径")
    sample_rate: int = Field(default=16000, description="采样率")
    params: Dict[str, Any] = Field(default_factory=dict, description="供应商参数")
    
    def to_str(self, sensitive_handling: bool = False) -> str:
        """转换为字符串"""
        if sensitive_handling:
            return f"VendorTTSConfig(api_key=***, dump={self.dump}, ...)"
        return f"VendorTTSConfig(api_key={self.api_key}, dump={self.dump}, ...)"
    
    def update_params(self) -> None:
        """更新参数"""
        # 实现参数更新逻辑
        pass
```

#### 4. 客户端开发

```python
# 2. 实现vendor_tts.py
import asyncio
from typing import AsyncIterator, Callable, Awaitable
from ten_runtime import AsyncTenEnv
from .config import VendorTTSConfig

class VendorTTS:
    """TTS客户端"""
    
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv, error_callback=None):
        self.config = config
        self.ten_env = ten_env
        self.error_callback = error_callback
        self._is_cancelled = False
    
    async def get(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """生成TTS音频"""
        # 实现具体的TTS逻辑
        yield b"mock_audio_data", 1, 100  # audio_data, event, ttfb_ms
        yield None, 2, None  # None, end_event, None
    
    def cancel(self):
        """取消请求"""
        self._is_cancelled = True
    
    async def stop(self):
        """停止客户端"""
        pass
```

#### 5. Extension开发

```python
# 3. 实现extension.py
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.struct import TTSTextInput
from .config import VendorTTSConfig
from .vendor_tts import VendorTTS

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """TTS Extension"""
    
    def __init__(self, name: str):
        super().__init__(name)
        self.config: VendorTTSConfig | None = None
        self.client: VendorTTS | None = None
    
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """初始化"""
        await super().on_init(ten_env)
        # 实现初始化逻辑
    
    async def request_tts(self, t: TTSTextInput) -> None:
        """处理TTS请求"""
        # 实现TTS请求处理逻辑
        pass
    
    def vendor(self) -> str:
        """返回供应商名称"""
        return "vendor_name"
    
    def synthesize_audio_sample_rate(self) -> int:
        """返回采样率"""
        return self.config.sample_rate if self.config else 16000
```

#### 6. 测试开发

```python
# 4. 实现测试
import pytest
from ten_runtime import ExtensionTester

class TestVendorTTS:
    """TTS测试"""
    
    @pytest.mark.asyncio
    async def test_basic_functionality(self):
        """基础功能测试"""
        # 实现测试逻辑
        pass
```

#### 7. 配置文件

```json
// 5. 配置manifest.json
{
  "name": "vendor_tts_python",
  "version": "1.0.0",
  "description": "Vendor TTS Extension",
  "api": {
    "name": "vendor_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "properties": {
          "key": {"type": "string"},
          "model_id": {"type": "string", "default": "default_model"}
        },
        "required": ["key"]
      }
    }
  }
}
```

```json
// 6. 配置property.json
{
  "key": "${VENDOR_TTS_API_KEY}",
  "model_id": "default_model",
  "dump": false,
  "dump_path": "./dump/"
}
```

### 调试技巧

#### 1. 日志调试

```python
import logging
from ten_runtime import AsyncTenEnv

class VendorTTS:
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        
        # 设置调试日志
        self.ten_env.log_debug("VendorTTS initialized")
        self.ten_env.log_info("Configuration loaded", category="VENDOR")
    
    async def get(self, text: str, request_id: str):
        self.ten_env.log_debug(f"Processing request: {request_id}")
        self.ten_env.log_info(f"Text length: {len(text)}", category="VENDOR")
        
        try:
            # TTS处理逻辑
            yield b"audio_data", 1, 100
        except Exception as e:
            self.ten_env.log_error(f"TTS processing error: {e}")
            raise
```

#### 2. 断点调试

```python
# 使用pdb进行断点调试
import pdb

class VendorTTS:
    async def get(self, text: str, request_id: str):
        # 设置断点
        pdb.set_trace()
        
        # 调试代码
        print(f"Processing text: {text}")
        print(f"Request ID: {request_id}")
        
        # 继续执行
        yield b"audio_data", 1, 100
```

#### 3. 性能分析

```python
import time
import cProfile
import pstats

class VendorTTS:
    async def get(self, text: str, request_id: str):
        start_time = time.time()
        
        # 性能分析
        profiler = cProfile.Profile()
        profiler.enable()
        
        try:
            # TTS处理逻辑
            yield b"audio_data", 1, 100
        finally:
            profiler.disable()
            
            # 输出性能统计
            stats = pstats.Stats(profiler)
            stats.sort_stats('cumulative')
            stats.print_stats(10)
            
            # 记录处理时间
            processing_time = time.time() - start_time
            self.ten_env.log_info(f"Processing time: {processing_time:.2f}s")
```

#### 4. 错误追踪

```python
import traceback
import sys

class VendorTTS:
    async def get(self, text: str, request_id: str):
        try:
            # TTS处理逻辑
            yield b"audio_data", 1, 100
        except Exception as e:
            # 详细错误信息
            error_info = {
                "error_type": type(e).__name__,
                "error_message": str(e),
                "traceback": traceback.format_exc(),
                "request_id": request_id,
                "text_length": len(text)
            }
            
            self.ten_env.log_error(f"TTS Error: {error_info}")
            
            # 发送错误事件
            if self.error_callback:
                await self.error_callback(request_id, e)
            
            raise
```

#### 5. 内存监控

```python
import psutil
import os

class VendorTTS:
    def __init__(self, config: VendorTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.process = psutil.Process(os.getpid())
    
    async def get(self, text: str, request_id: str):
        # 记录内存使用
        memory_info = self.process.memory_info()
        self.ten_env.log_debug(f"Memory usage: {memory_info.rss / 1024 / 1024:.2f} MB")
        
        try:
            # TTS处理逻辑
            yield b"audio_data", 1, 100
        finally:
            # 记录处理后的内存使用
            memory_info = self.process.memory_info()
            self.ten_env.log_debug(f"Memory usage after: {memory_info.rss / 1024 / 1024:.2f} MB")
```

### 测试策略

#### 1. 单元测试

```python
import pytest
from unittest.mock import Mock, patch, AsyncMock

class TestVendorTTS:
    """单元测试"""
    
    @pytest.fixture
    def mock_config(self):
        """Mock配置"""
        return VendorTTSConfig(
            api_key="test_key",
            dump=False,
            sample_rate=16000
        )
    
    @pytest.fixture
    def mock_ten_env(self):
        """Mock TenEnv"""
        mock_env = Mock()
        mock_env.log_debug = Mock()
        mock_env.log_info = Mock()
        mock_env.log_error = Mock()
        return mock_env
    
    @pytest.mark.asyncio
    async def test_get_method(self, mock_config, mock_ten_env):
        """测试get方法"""
        client = VendorTTS(mock_config, mock_ten_env)
        
        # Mock外部依赖
        with patch('vendor_tts.external_api_call') as mock_api:
            mock_api.return_value = b"test_audio_data"
            
            # 执行测试
            audio_generator = client.get("test text", "test_request")
            audio_data, event, ttfb_ms = await audio_generator.__anext__()
            
            # 验证结果
            assert audio_data == b"test_audio_data"
            assert event == 1
            assert ttfb_ms == 100
```

#### 2. 集成测试

```python
class TestIntegration:
    """集成测试"""
    
    @pytest.mark.asyncio
    async def test_end_to_end(self):
        """端到端测试"""
        # 创建真实环境
        tester = ExtensionTesterDump()
        
        # 启动Extension
        await tester.start(fake_app, config_path)
        
        # 发送请求
        tts_input = TTSTextInput(
            text="Hello, world!",
            request_id="integration_test",
            text_input_end=True
        )
        await tester.send_tts_text_input(tts_input)
        
        # 等待处理
        await asyncio.sleep(2)
        
        # 验证结果
        assert len(tester.received_audio_chunks) > 0
        assert tester.audio_end_received
        
        await tester.stop()
```

#### 3. 压力测试

```python
class TestStress:
    """压力测试"""
    
    @pytest.mark.asyncio
    async def test_concurrent_requests(self):
        """并发请求测试"""
        tester = ExtensionTesterDump()
        await tester.start(fake_app, config_path)
        
        # 并发发送100个请求
        tasks = []
        for i in range(100):
            tts_input = TTSTextInput(
                text=f"Request {i}",
                request_id=f"stress_test_{i}",
                text_input_end=True
            )
            task = asyncio.create_task(tester.send_tts_text_input(tts_input))
            tasks.append(task)
        
        # 等待所有请求完成
        await asyncio.gather(*tasks)
        await asyncio.sleep(5)
        
        # 验证结果
        assert len(tester.received_audio_chunks) >= 100
        
        await tester.stop()
```

### 部署和发布

#### 1. 版本管理

```bash
# 使用语义化版本号
# 主版本号.次版本号.修订号
# 1.0.0 - 初始版本
# 1.1.0 - 新功能
# 1.1.1 -  bug修复

# 更新版本号
echo "1.0.0" > VERSION
```

#### 2. 构建脚本

```bash
#!/bin/bash
# build.sh

set -e

# 清理旧文件
rm -rf dist/
rm -rf build/

# 运行测试
python -m pytest tests/ -v

# 构建包
python setup.py sdist bdist_wheel

# 检查包
twine check dist/*

echo "Build completed successfully!"
```

#### 3. 发布流程

```bash
#!/bin/bash
# release.sh

set -e

# 检查版本
VERSION=$(cat VERSION)
echo "Releasing version $VERSION"

# 更新manifest.json
jq --arg version "$VERSION" '.version = $version' manifest.json > manifest.json.tmp
mv manifest.json.tmp manifest.json

# 提交更改
git add .
git commit -m "Release version $VERSION"
git tag "v$VERSION"

# 推送到仓库
git push origin main
git push origin "v$VERSION"

echo "Release $VERSION completed!"
```

### 最佳实践总结

1. **代码质量**：使用类型提示、文档字符串和代码格式化
2. **错误处理**：实现全面的错误处理和恢复机制
3. **测试覆盖**：确保高测试覆盖率，包括单元测试和集成测试
4. **性能优化**：监控性能指标，优化关键路径
5. **安全考虑**：保护敏感信息，使用环境变量
6. **文档维护**：保持文档与代码同步更新
7. **版本控制**：使用语义化版本号，维护变更日志
8. **持续集成**：设置CI/CD流水线，自动化测试和部署

## 示例代码

### 完整实现示例

以下是一个完整的TTS Extension实现示例，展示了如何实现一个基于HTTP的TTS客户端。

#### 1. 项目结构

```
vendor_http_tts_python/
├── __init__.py
├── config.py
├── vendor_tts.py
├── extension.py
├── requirements.txt
├── manifest.json
├── property.json
├── README.md
└── tests/
    ├── __init__.py
    ├── conftest.py
    ├── test_basic.py
    ├── test_error_msg.py
    ├── test_params.py
    └── test_robustness.py
```

#### 2. config.py - 配置类实现

```python
from pydantic import BaseModel, Field, validator
from typing import Dict, Any, Optional
import os

class VendorTTSConfig(BaseModel):
    """Vendor TTS配置类"""
    
    # 必需参数
    api_key: str = Field(..., description="API密钥")
    
    # 可选参数
    dump: bool = Field(default=False, description="是否启用音频dump")
    dump_path: str = Field(default="./dump/", description="音频dump路径")
    sample_rate: int = Field(default=16000, description="音频采样率")
    
    # 供应商特定参数
    params: Dict[str, Any] = Field(default_factory=dict, description="供应商参数")
    
    # 黑名单键（不记录到日志）
    black_list_keys: set = Field(default={"api_key", "credentials"}, description="敏感信息键")
    
    @validator('sample_rate')
    def validate_sample_rate(cls, v):
        """验证采样率"""
        if v not in [8000, 16000, 24000, 48000]:
            raise ValueError(f"Unsupported sample rate: {v}")
        return v
    
    @validator('dump_path')
    def validate_dump_path(cls, v):
        """验证dump路径"""
        if v and not os.path.exists(v):
            os.makedirs(v, exist_ok=True)
        return v
    
    def to_str(self, sensitive_handling: bool = False) -> str:
        """转换为字符串表示"""
        if sensitive_handling:
            # 敏感信息处理模式
            filtered_params = {
                k: "***" if k in self.black_list_keys else v
                for k, v in self.params.items()
            }
            return f"VendorTTSConfig(api_key=***, dump={self.dump}, params={filtered_params})"
        else:
            return f"VendorTTSConfig(api_key={self.api_key}, dump={self.dump}, params={self.params})"
    
    def update_params(self) -> None:
        """更新参数配置"""
        # 设置默认参数
        default_params = {
            "model_id": "default_model",
            "voice_id": "default_voice",
            "speaking_rate": 1.0,
            "pitch": 0.0,
            "volume_gain_db": 0.0,
            "timeout": 30,
            "max_retries": 3,
            "retry_delay": 1.0,
            "chunk_size": 4096,
        }
        
        # 更新参数，保留用户设置的值
        for key, default_value in default_params.items():
            if key not in self.params:
                self.params[key] = default_value
        
        # 验证参数范围
        if self.params["speaking_rate"] < 0.25 or self.params["speaking_rate"] > 4.0:
            self.params["speaking_rate"] = 1.0
        
        if self.params["pitch"] < -20.0 or self.params["pitch"] > 20.0:
            self.params["pitch"] = 0.0
        
        if self.params["volume_gain_db"] < -96.0 or self.params["volume_gain_db"] > 16.0:
            self.params["volume_gain_db"] = 0.0
```

#### 3. vendor_tts.py - 客户端实现

```python
import asyncio
import json
import time
from typing import AsyncIterator, Callable, Awaitable, Optional
from httpx import AsyncClient, Timeout, Limits, HTTPError
from ten_runtime import AsyncTenEnv
from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
from .config import VendorTTSConfig

# 事件类型定义
EVENT_TTS_RESPONSE = 1
EVENT_TTS_REQUEST_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5

class VendorTTSClient:
    """Vendor HTTP TTS客户端"""
    
    def __init__(
        self,
        config: VendorTTSConfig,
        ten_env: AsyncTenEnv,
        error_callback: Optional[Callable[[str, ModuleError], Awaitable[None]]] = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.error_callback = error_callback
        self._is_cancelled = False
        
        # HTTP客户端配置
        self.endpoint = self.config.params.get("http_url", "https://api.vendor.com/v1/tts")
        self.headers = {
            "Authorization": f"Bearer {self.config.api_key}",
            "Content-Type": "application/json",
            "Accept": "audio/pcm",
            "User-Agent": "VendorTTS-Python/1.0.0",
        }
        
        # 创建HTTP客户端
        self.client = AsyncClient(
            timeout=Timeout(timeout=self.config.params.get("timeout", 30)),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
            follow_redirects=True,
        )
        
        self.ten_env.log_debug("VendorTTSClient initialized")
    
    async def get(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
        """处理TTS请求"""
        self._is_cancelled = False
        start_time = time.time()
        ttfb_ms = None
        
        if not self.client:
            error_msg = "HTTP client not initialized"
            yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, None
            return
        
        try:
            # 构建请求数据
            request_data = {
                "text": text,
                "model_id": self.config.params.get("model_id"),
                "voice_id": self.config.params.get("voice_id"),
                "speaking_rate": self.config.params.get("speaking_rate"),
                "pitch": self.config.params.get("pitch"),
                "volume_gain_db": self.config.params.get("volume_gain_db"),
                "sample_rate": self.config.sample_rate,
            }
            
            self.ten_env.log_debug(f"Sending TTS request: {len(text)} characters")
            
            # 发送HTTP请求
            async with self.client.stream(
                "POST",
                self.endpoint,
                headers=self.headers,
                json=request_data,
            ) as response:
                # 计算TTFB
                if ttfb_ms is None:
                    ttfb_ms = int((time.time() - start_time) * 1000)
                
                # 检查响应状态
                if response.status_code != 200:
                    error_msg = f"HTTP {response.status_code}: {response.reason_phrase}"
                    self.ten_env.log_error(f"HTTP error: {error_msg}")
                    
                    if response.status_code == 401:
                        yield error_msg.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR, ttfb_ms
                    else:
                        yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
                    return
                
                # 处理流式响应
                chunk_size = self.config.params.get("chunk_size", 4096)
                audio_received = False
                
                async for chunk in response.aiter_bytes(chunk_size=chunk_size):
                    if self._is_cancelled:
                        self.ten_env.log_debug("Request cancelled, stopping audio processing")
                        yield None, EVENT_TTS_FLUSH, ttfb_ms
                        break
                    
                    if len(chunk) > 0:
                        audio_received = True
                        yield bytes(chunk), EVENT_TTS_RESPONSE, ttfb_ms
                        ttfb_ms = None  # 只在第一个chunk中包含TTFB
                    else:
                        # 空chunk表示结束
                        break
                
                if not self._is_cancelled and audio_received:
                    yield None, EVENT_TTS_REQUEST_END, None
                elif not audio_received:
                    error_msg = "No audio data received"
                    yield error_msg.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
        
        except HTTPError as e:
            error_message = f"HTTP error: {e}"
            self.ten_env.log_error(f"vendor_error: {error_message}")
            
            if self._is_authentication_error(str(e)):
                yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR, ttfb_ms
            else:
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
        
        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(f"vendor_error: {error_message}")
            
            if self._is_authentication_error(error_message):
                yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR, ttfb_ms
            else:
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR, ttfb_ms
    
    def _is_authentication_error(self, error_message: str) -> bool:
        """检查是否为认证错误"""
        auth_keywords = [
            "401", "unauthorized", "authentication", "credentials",
            "invalid token", "expired token", "forbidden", "access denied"
        ]
        return any(keyword in error_message.lower() for keyword in auth_keywords)
    
    def _is_retryable_error(self, error_message: str) -> bool:
        """检查是否为可重试错误"""
        retryable_keywords = [
            "timeout", "connection", "503", "502", "504",
            "service unavailable", "bad gateway", "gateway timeout",
            "network", "dns", "socket"
        ]
        return any(keyword in error_message.lower() for keyword in retryable_keywords)
    
    def cancel(self) -> None:
        """取消当前请求"""
        self._is_cancelled = True
        self.ten_env.log_debug("VendorTTSClient: cancel() called")
    
    async def stop(self) -> None:
        """停止客户端"""
        if self.client:
            await self.client.aclose()
            self.client = None
            self.ten_env.log_debug("VendorTTSClient stopped")
```

#### 4. extension.py - Extension实现

```python
import asyncio
from datetime import datetime
import os
import traceback
from typing import Tuple

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .config import VendorTTSConfig
from .vendor_tts import VendorTTSClient, EVENT_TTS_RESPONSE, EVENT_TTS_REQUEST_END, EVENT_TTS_ERROR, EVENT_TTS_INVALID_KEY_ERROR
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR

class VendorTTS Extension(AsyncTTS2BaseExtension):
    """Vendor TTS Extension实现"""
    
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: VendorTTSConfig | None = None
        self.client: VendorTTSClient | None = None
        self.current_request_id: str | None = None
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.recorder_map: dict[str, PCMWriter] = {}
        self.last_complete_request_id: str | None = None
        self._flush_requested = False
    
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Extension初始化"""
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("VendorTTS extension on_init started")
            
            # 获取配置
            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = VendorTTSConfig.model_validate_json(config_json)
                self.config.update_params()
                
                ten_env.log_info(
                    f"config: {self.config.to_str(sensitive_handling=True)}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
                
                # 验证必需参数
                if not self.config.api_key:
                    raise ValueError("API key is required")
            
            # 创建错误回调
            async def error_callback(request_id: str, error: ModuleError):
                target_request_id = request_id if request_id else self.current_request_id or ""
                await self.send_tts_error(request_id=target_request_id, error=error)
                
                if error.code == ModuleErrorCode.FATAL_ERROR:
                    ten_env.log_error(f"Fatal error occurred: {error.message}")
                    await self.client.stop()
                    await self.on_stop(self.ten_env)
            
            # 创建客户端
            self.client = VendorTTSClient(
                config=self.config,
                ten_env=ten_env,
                error_callback=error_callback,
            )
            
            ten_env.log_debug("VendorTTS extension on_init completed")
        
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
    
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Extension启动"""
        await super().on_start(ten_env)
        ten_env.log_debug("VendorTTS extension on_start")
    
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Extension停止"""
        ten_env.log_debug("VendorTTS extension on_stop started")
        
        # 停止客户端
        if self.client:
            try:
                await self.client.stop()
            except Exception as e:
                ten_env.log_error(f"Error stopping VendorTTS client: {e}")
            finally:
                self.client = None
        
        # 清理PCMWriter
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_debug(f"Flushed PCMWriter for request_id: {request_id}")
            except Exception as e:
                ten_env.log_error(f"Error flushing PCMWriter for request_id {request_id}: {e}")
        
        # 清理状态
        self.recorder_map.clear()
        self.last_complete_request_id = None
        
        await super().on_stop(ten_env)
        ten_env.log_debug("VendorTTS extension on_stop completed")
    
    async def request_tts(self, t: TTSTextInput) -> None:
        """处理TTS请求"""
        try:
            if not self.client or not self.config:
                raise RuntimeError("Extension is not initialized properly.")
            
            # 检查请求ID是否已完成
            if self.last_complete_request_id == t.request_id:
                self.ten_env.log_debug(f"Request ID {t.request_id} has already been completed, ignoring")
                return
            
            # 处理新请求ID
            if t.request_id != self.current_request_id:
                self.current_request_id = t.request_id
                self._reset_request_state()
                self._flush_requested = False
                
                # 创建PCMWriter（如果启用dump）
                await self._setup_pcm_writer(t.request_id)
            
            # 处理文本
            if t.text.strip() != "":
                await self._process_text_request(t)
            
            # 处理请求结束
            if t.text_input_end:
                self.current_request_finished = True
                await self.handle_completed_request(TTSAudioEndReason.REQUEST_END)
        
        except Exception as e:
            self.ten_env.log_error(f"Error in request_tts: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id=self.current_request_id or t.request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
    
    async def _process_text_request(self, t: TTSTextInput) -> None:
        """处理文本请求"""
        self.ten_env.log_debug(
            f"send_text_to_tts_server: {t.text} of request_id: {t.request_id}",
            category=LOG_CATEGORY_VENDOR,
        )
        
        # 获取音频生成器
        audio_generator = self.client.get(t.text, t.request_id)
        
        try:
            async for audio_chunk, event, ttfb_ms in audio_generator:
                # 检查是否被刷新
                if self._flush_requested:
                    self.ten_env.log_debug("Flush requested, stopping audio processing")
                    break
                
                if event == EVENT_TTS_RESPONSE and audio_chunk:
                    await self._handle_audio_chunk(audio_chunk, ttfb_ms)
                elif event == EVENT_TTS_REQUEST_END:
                    break
                elif event == EVENT_TTS_INVALID_KEY_ERROR:
                    await self._handle_authentication_error(audio_chunk)
                    return
                elif event == EVENT_TTS_ERROR:
                    error_msg = audio_chunk.decode("utf-8") if audio_chunk else "Unknown error"
                    raise RuntimeError(error_msg)
        
        except Exception as e:
            self.ten_env.log_error(f"Error in audio processing: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id=self.current_request_id or t.request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
        finally:
            # 确保生成器被正确关闭
            try:
                await audio_generator.aclose()
            except Exception as e:
                self.ten_env.log_error(f"Error closing audio generator: {e}")
    
    async def _handle_audio_chunk(self, audio_chunk: bytes, ttfb_ms: int = None) -> None:
        """处理音频数据块"""
        self.total_audio_bytes += len(audio_chunk)
        
        # 计算音频时长
        duration_ms = self._calculate_audio_duration_ms()
        
        self.ten_env.log_debug(
            f"receive_audio: duration: {duration_ms} of request id: {self.current_request_id}",
            category=LOG_CATEGORY_VENDOR,
        )
        
        # 发送音频开始事件（首次）
        if self.sent_ts is None and self.current_request_id:
            self.sent_ts = datetime.now()
            await self.send_tts_audio_start(request_id=self.current_request_id)
            
            # 发送TTFB指标
            if ttfb_ms is not None:
                extra_metadata = self._get_extra_metadata()
                await self.send_tts_ttfb_metrics(
                    request_id=self.current_request_id,
                    ttfb_ms=ttfb_ms,
                    extra_metadata=extra_metadata,
                )
        
        # 写入PCM文件（如果启用dump）
        if self.config.dump and self.current_request_id in self.recorder_map:
            await self.recorder_map[self.current_request_id].write(audio_chunk)
        
        # 发送音频数据
        await self.send_tts_audio_data(audio_chunk)
    
    def _calculate_audio_duration_ms(self) -> int:
        """计算音频时长（毫秒）"""
        if self.config is None:
            return 0
        
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)
    
    def _get_extra_metadata(self) -> dict:
        """获取额外元数据"""
        return {
            "voice_id": self.config.params.get("voice_id", ""),
            "model_id": self.config.params.get("model_id", ""),
        }
    
    async def handle_completed_request(self, reason: TTSAudioEndReason):
        """处理请求完成"""
        # 更新完成请求ID
        if self.last_complete_request_id == self.current_request_id:
            self.ten_env.log_debug(f"{self.current_request_id} was completed, skip.")
            return
        
        self.last_complete_request_id = self.current_request_id
        self.ten_env.log_debug(f"update last_complete_request_id to: {self.current_request_id}")
        
        # 刷新PCMWriter
        if self.config.dump and self.current_request_id in self.recorder_map:
            try:
                await self.recorder_map[self.current_request_id].flush()
                self.ten_env.log_debug(f"Flushed PCMWriter for request_id: {self.current_request_id}")
            except Exception as e:
                self.ten_env.log_error(f"Error flushing PCMWriter: {e}")
        
        # 发送音频结束事件
        request_event_interval = 0
        if self.sent_ts is not None:
            request_event_interval = int(
                (datetime.now() - self.sent_ts).total_seconds() * 1000
            )
        
        duration_ms = self._calculate_audio_duration_ms()
        
        await self.send_tts_audio_end(
            request_id=self.current_request_id or "",
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=duration_ms,
            reason=reason,
        )
        
        self.ten_env.log_debug(
            f"Sent tts_audio_end with {reason.name} reason for request_id: {self.current_request_id}"
        )
        
        # 重置状态
        self.sent_ts = None
        self.total_audio_bytes = 0
    
    async def cancel_tts(self) -> None:
        """取消TTS请求"""
        self._flush_requested = True
        
        try:
            if self.client is not None:
                self.ten_env.log_info("Flushing VendorTTS client")
                self.client.cancel()
            else:
                self.ten_env.log_warn("Client is not initialized, skipping reset")
        
        except Exception as e:
            self.ten_env.log_error(f"Error in handle_flush: {e}")
            await self.send_tts_error(
                request_id=self.current_request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
        
        await self.handle_completed_request(TTSAudioEndReason.INTERRUPTED)
    
    async def _setup_pcm_writer(self, request_id: str) -> None:
        """设置PCM文件写入器"""
        if not self.config.dump:
            return
        
        # 清理旧的PCMWriter
        old_request_ids = [rid for rid in self.recorder_map.keys() if rid != request_id]
        for old_rid in old_request_ids:
            try:
                await self.recorder_map[old_rid].flush()
                del self.recorder_map[old_rid]
                self.ten_env.log_debug(f"Cleaned up old PCMWriter for request_id: {old_rid}")
            except Exception as e:
                self.ten_env.log_error(f"Error cleaning up PCMWriter for request_id {old_rid}: {e}")
        
        # 创建新的PCMWriter
        if request_id not in self.recorder_map:
            dump_file_path = os.path.join(
                self.config.dump_path,
                f"vendor_dump_{request_id}.pcm",
            )
            self.recorder_map[request_id] = PCMWriter(dump_file_path)
            self.ten_env.log_info(f"Created PCMWriter for request_id: {request_id}, file: {dump_file_path}")
    
    def _reset_request_state(self) -> None:
        """重置请求状态"""
        self.sent_ts = None
        self.total_audio_bytes = 0
        self.current_request_finished = False
    
    async def _handle_authentication_error(self, error_data: bytes) -> None:
        """处理认证错误"""
        error_msg = error_data.decode("utf-8") if error_data else "Authentication failed"
        self.ten_env.log_error(f"Authentication error: {error_msg}")
        
        await self.send_tts_error(
            request_id=self.current_request_id or "",
            error=ModuleError(
                message=error_msg,
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
    
    def vendor(self) -> str:
        """返回供应商名称"""
        return "vendor_http"
    
    def synthesize_audio_sample_rate(self) -> int:
        """返回音频采样率"""
        return self.config.sample_rate if self.config else 16000
    
    def synthesize_audio_channels(self) -> int:
        """返回音频声道数"""
        return 1
    
    def synthesize_audio_sample_width(self) -> int:
        """返回音频采样宽度"""
        return 2
```

#### 5. requirements.txt - 依赖文件

```txt
pydantic>=2.0.0
httpx>=0.24.0
ten_ai_base>=1.0.0
ten_runtime>=1.0.0
```

#### 6. manifest.json - Extension清单

```json
{
  "name": "vendor_http_tts_python",
  "version": "1.0.0",
  "description": "Vendor HTTP TTS Extension for TEN Framework",
  "author": "Your Name",
  "license": "MIT",
  "dependencies": {
    "ten_ai_base": ">=1.0.0",
    "ten_runtime": ">=1.0.0"
  },
  "package": {
    "include": [
      "vendor_tts.py",
      "config.py",
      "extension.py",
      "requirements.txt"
    ],
    "exclude": [
      "tests/",
      "*.pyc",
      "__pycache__/"
    ]
  },
  "api": {
    "name": "vendor_http_tts",
    "version": "1.0.0",
    "properties": {
      "params": {
        "type": "object",
        "description": "TTS configuration parameters",
        "properties": {
          "key": {
            "type": "string",
            "description": "API key for authentication",
            "minLength": 1
          },
          "model_id": {
            "type": "string",
            "description": "TTS model identifier",
            "default": "default_model"
          },
          "voice_id": {
            "type": "string",
            "description": "Voice identifier",
            "default": "default_voice"
          },
          "http_url": {
            "type": "string",
            "description": "HTTP endpoint URL",
            "default": "https://api.vendor.com/v1/tts"
          },
          "timeout": {
            "type": "integer",
            "description": "Request timeout in seconds",
            "minimum": 5,
            "maximum": 300,
            "default": 30
          },
          "max_retries": {
            "type": "integer",
            "description": "Maximum retry attempts",
            "minimum": 0,
            "maximum": 5,
            "default": 3
          },
          "retry_delay": {
            "type": "number",
            "description": "Retry delay in seconds",
            "minimum": 0.1,
            "maximum": 10.0,
            "default": 1.0
          },
          "chunk_size": {
            "type": "integer",
            "description": "Response chunk size",
            "minimum": 1024,
            "maximum": 65536,
            "default": 4096
          },
          "speaking_rate": {
            "type": "number",
            "description": "Speaking rate",
            "minimum": 0.25,
            "maximum": 4.0,
            "default": 1.0
          },
          "pitch": {
            "type": "number",
            "description": "Voice pitch",
            "minimum": -20.0,
            "maximum": 20.0,
            "default": 0.0
          },
          "volume_gain_db": {
            "type": "number",
            "description": "Volume gain in dB",
            "minimum": -96.0,
            "maximum": 16.0,
            "default": 0.0
          },
          "dump": {
            "type": "boolean",
            "description": "Enable audio dump",
            "default": false
          },
          "dump_path": {
            "type": "string",
            "description": "Audio dump file path",
            "default": "./dump/"
          }
        },
        "required": ["key"]
      }
    }
  }
}
```

#### 7. property.json - 默认配置

```json
{
  "dump": false,
  "dump_path": "./dump/",
  "key": "${VENDOR_TTS_API_KEY}",
  "model_id": "default_model",
  "voice_id": "default_voice",
  "http_url": "https://api.vendor.com/v1/tts",
  "timeout": 30,
  "max_retries": 3,
  "retry_delay": 1.0,
  "chunk_size": 4096,
  "speaking_rate": 1.0,
  "pitch": 0.0,
  "volume_gain_db": 0.0,
  "output_format": "pcm_16000"
}
```

#### 8. 测试示例

```python
# tests/test_basic.py
import pytest
import asyncio
from ten_runtime import ExtensionTester
from ten_ai_base.struct import TTSTextInput

class ExtensionTesterDump(ExtensionTester):
    """支持dump的Extension测试器"""
    
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []
        self.error_received = False
        self.ttfb_metrics_received = False
    
    async def on_audio_frame(self, audio_frame):
        """处理音频帧"""
        self.received_audio_chunks.append(audio_frame.get_data())
    
    async def on_data(self, data):
        """处理数据"""
        data_name = data.get_name()
        if data_name == "tts_audio_end":
            self.audio_end_received = True
        elif data_name == "metrics":
            self.ttfb_metrics_received = True
        elif data_name == "error":
            self.error_received = True

class TestVendorTTS:
    """Vendor TTS测试"""
    
    @pytest.mark.asyncio
    async def test_basic_functionality(self, fake_app, test_config):
        """基础功能测试"""
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = os.path.join(temp_dir, "property.json")
            with open(config_path, 'w') as f:
                json.dump(test_config, f)
            
            tester = ExtensionTesterDump()
            await tester.start(fake_app, config_path)
            
            # 创建TTS请求
            tts_input = TTSTextInput(
                text="Hello, world!",
                request_id="test_request_1",
                text_input_end=True
            )
            
            # 发送请求
            await tester.send_tts_text_input(tts_input)
            
            # 等待处理完成
            await asyncio.sleep(2)
            
            # 验证结果
            assert len(tester.received_audio_chunks) > 0
            assert tester.audio_end_received
            
            await tester.stop()
```

### 关键代码片段

#### 1. 异步生成器模式

```python
async def get(self, text: str, request_id: str) -> AsyncIterator[tuple[bytes | None, int, int | None]]:
    """异步生成器模式处理TTS请求"""
    try:
        # 发送请求
        async with self.client.stream("POST", self.endpoint, json=request_data) as response:
            # 处理流式响应
            async for chunk in response.aiter_bytes():
                if self._is_cancelled:
                    yield None, EVENT_TTS_FLUSH, None
                    break
                
                if len(chunk) > 0:
                    yield bytes(chunk), EVENT_TTS_RESPONSE, ttfb_ms
                else:
                    yield None, EVENT_TTS_REQUEST_END, None
    except Exception as e:
        yield str(e).encode(), EVENT_TTS_ERROR, None
```

#### 2. 错误处理模式

```python
def _is_authentication_error(self, error_message: str) -> bool:
    """错误分类处理"""
    auth_keywords = ["401", "unauthorized", "authentication"]
    return any(keyword in error_message.lower() for keyword in auth_keywords)

async def _handle_error(self, error_message: str, error_code: int = 0):
    """统一错误处理"""
    if self.error_callback:
        error_info = ModuleErrorVendorInfo(
            vendor="vendor_name",
            code=str(error_code),
            message=error_message,
        )
        module_error = ModuleError(
            message=error_message,
            module=ModuleType.TTS,
            code=ModuleErrorCode.NON_FATAL_ERROR,
            vendor_info=error_info,
        )
        await self.error_callback("", module_error)
```

#### 3. 资源管理模式

```python
async def stop(self) -> None:
    """资源清理模式"""
    if self.client:
        await self.client.aclose()
        self.client = None
    
    # 清理PCMWriter
    for recorder in self.recorder_map.values():
        await recorder.flush()
    self.recorder_map.clear()
```

#### 4. 配置验证模式

```python
@validator('sample_rate')
def validate_sample_rate(cls, v):
    """参数验证模式"""
    if v not in [8000, 16000, 24000, 48000]:
        raise ValueError(f"Unsupported sample rate: {v}")
    return v

def update_params(self) -> None:
    """参数更新模式"""
    default_params = {"timeout": 30, "max_retries": 3}
    for key, default_value in default_params.items():
        if key not in self.params:
            self.params[key] = default_value
```

这个完整的示例展示了如何实现一个生产级别的TTS Extension，包含了所有必要的组件和最佳实践。开发者可以参考这个示例来实现自己的TTS供应商客户端。

