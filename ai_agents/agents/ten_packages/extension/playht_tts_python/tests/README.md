# PlayHT TTS Python Extension Tests

这个目录包含了 PlayHT TTS Python 扩展的完整单元测试套件。

## 测试文件结构

### 核心测试文件

- **`test_playht_tts_mock.py`** - PlayHT TTS 扩展的集成测试
  - 测试扩展的完整工作流程
  - 模拟 PlayHT API 调用
  - 测试成功和错误场景
  - 测试音频转储功能

- **`test_playht_client.py`** - PlayHTTTS 客户端类的单元测试
  - 测试客户端初始化和配置
  - 测试音频合成功能
  - 测试重试机制
  - 测试错误处理

- **`test_config.py`** - 配置类的单元测试
  - 测试 PlayHTParams 参数验证
  - 测试 PlayHTTTSConfig 配置管理
  - 测试序列化和反序列化
  - 测试加密字段处理

- **`test_error_handling.py`** - 错误处理和重试机制的测试
  - 测试重试装饰器功能
  - 测试各种错误场景
  - 测试网络错误处理
  - 测试认证和限流错误

- **`test_utils.py`** - 工具函数的单元测试
  - 测试加密序列化器
  - 测试重试上下文管理器

### 测试运行器

- **`run_tests.py`** - 测试运行脚本
  - 可以运行所有测试或特定测试模块
  - 提供详细的测试输出

## 运行测试

### 运行所有测试

```bash
cd /app/agents/ten_packages/extension/playht_tts_python/tests
python run_tests.py
```

### 运行特定测试模块

```bash
python run_tests.py test_playht_client
python run_tests.py test_config
python run_tests.py test_error_handling
```

### 使用 unittest 直接运行

```bash
python -m unittest test_playht_client.py
python -m unittest test_config.py
python -m unittest test_error_handling.py
python -m unittest test_utils.py
```

## 测试覆盖范围

### 功能测试
- ✅ PlayHTTTS 客户端初始化和配置
- ✅ 音频合成功能
- ✅ 参数验证和转换
- ✅ 配置序列化/反序列化
- ✅ 错误处理和重试机制
- ✅ 加密字段处理

### 集成测试
- ✅ 扩展完整工作流程
- ✅ 模拟 API 调用
- ✅ 音频数据流处理
- ✅ 错误场景处理
- ✅ 音频转储功能

### 边界情况测试
- ✅ 空文本处理
- ✅ 网络错误处理
- ✅ 认证错误处理
- ✅ 限流错误处理
- ✅ 超时错误处理
- ✅ 配置验证错误

## 测试数据

测试使用模拟数据，不依赖真实的 PlayHT API：
- 使用 `unittest.mock` 模拟 API 调用
- 使用假的 API 密钥和用户 ID
- 模拟音频数据块
- 模拟各种错误场景

## 注意事项

1. 所有测试都是单元测试，不依赖外部服务
2. 测试使用模拟对象，确保测试的稳定性和速度
3. 测试覆盖了主要的成功和失败场景
4. 测试数据使用加密序列化器保护敏感信息
5. 测试遵循 AAA 模式（Arrange, Act, Assert）

## 扩展测试

如果需要添加新的测试：

1. 创建新的测试文件，命名格式为 `test_*.py`
2. 继承适当的测试基类（`unittest.TestCase` 或 `unittest.IsolatedAsyncioTestCase`）
3. 使用描述性的测试方法名
4. 添加适当的文档字符串
5. 确保测试是独立的，不依赖其他测试的状态
