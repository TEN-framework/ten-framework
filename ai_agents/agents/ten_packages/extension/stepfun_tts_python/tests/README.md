# StepFun TTS Python Extension Tests

这个目录包含了 StepFun TTS Python 扩展的测试文件。

## 测试文件结构

```
tests/
├── test_basic.py          # 主要测试文件
├── test_error_debug.py    # 错误调试测试文件
├── bin/
│   └── start             # 测试启动脚本
└── README.md             # 测试说明文档
```

## 测试内容

### 1. 基本功能测试 (`test_basic_functionality`)
- 测试基本的 TTS 文本转语音功能
- 验证音频数据生成和传输
- 检查音频结束事件

### 2. 音频转储测试 (`test_dump_functionality`)
- 测试音频文件转储功能
- 验证 PCM 文件生成
- 检查音频数据完整性

### 3. 文本输入结束测试 (`test_text_input_end`)
- 测试多个 TTS 请求的处理
- 验证请求序列的正确处理
- 检查音频结束事件的正确计数

### 4. 刷新功能测试 (`test_flush_functionality`)
- 测试 TTS 刷新机制
- 验证音频开始和结束事件
- 检查刷新请求的处理

### 5. 错误处理测试 (`test_error_handling`)
- 测试异常情况的处理
- 验证错误事件的发送
- 检查错误恢复机制

### 6. 错误调试信息测试 (`test_error_debug_information`)
- 测试详细的错误信息提供
- 验证认证错误的详细信息
- 检查 API 密钥错误的处理

### 7. 错误堆栈跟踪测试 (`test_error_debug_stack_trace`)
- 测试错误堆栈跟踪信息
- 验证嵌套错误的处理
- 检查错误链的完整性

### 8. 请求上下文错误测试 (`test_error_debug_request_context`)
- 测试请求上下文在错误中的提供
- 验证文本内容和请求 ID 的错误信息
- 检查错误调试信息的完整性

### 9. 音色标签错误测试 (`test_error_debug_voice_label_context`)
- 测试音色标签配置错误的处理
- 验证语言、情感、语速错误的详细信息
- 检查音色标签上下文的错误信息

### 10. 网络错误测试 (`test_error_debug_network_error`)
- 测试网络连接错误的处理
- 验证 API 端点错误的详细信息
- 检查网络错误的重试和恢复机制

## 运行测试

### 使用测试脚本
```bash
cd tests
./bin/start
```

### 直接使用 pytest
```bash
cd tests
python -m pytest test_basic.py test_error_debug.py -v
```

### 运行特定测试
```bash
cd tests
python -m pytest test_basic.py::test_basic_functionality -v
python -m pytest test_error_debug.py::test_error_debug_information -v
```

## 测试配置

测试使用模拟的 StepFun TTS 客户端，不需要真实的 API 密钥。测试配置包括：

- **API 密钥**: `fake_api_key_for_mock_testing`
- **模型**: `step-tts-mini`
- **音色**: `cixingnansheng`
- **音频格式**: `mp3`
- **采样率**: `24000`

## 测试数据

测试使用中文文本：
- 基本测试: "智能阶跃，十倍每个人的可能"
- 多请求测试: "第二个请求"

## 模拟数据

测试使用模拟的音频数据：
- `b"fake_audio_data"` - 模拟音频数据
- `b"fake_audio_data_1"`, `b"fake_audio_data_2"` - 多个音频块

## 事件类型

测试验证以下事件类型：
- `EVENT_TTS_RESPONSE` (1) - TTS 响应
- `EVENT_TTS_REQUEST_END` (2) - 请求结束
- `EVENT_TTS_ERROR` (3) - 错误事件
- `EVENT_TTS_INVALID_KEY_ERROR` (4) - API 密钥错误

## 注意事项

1. 测试需要 TEN Framework 环境
2. 测试使用模拟客户端，不进行真实的 API 调用
3. 音频转储测试会在 `./dump/` 目录创建测试文件
4. 所有测试都包含完整的错误处理和断言验证

## 扩展测试

要添加新的测试：

1. 在 `test_basic.py` 中添加新的测试类
2. 继承 `ExtensionTester` 类
3. 实现必要的回调方法
4. 添加对应的测试函数
5. 使用 `@patch` 装饰器模拟 StepFunTTS 客户端
