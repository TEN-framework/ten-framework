# DingTalk Bot 扩展重命名方案

## 重命名信息
- **原名称**: `dingtalk_bot`
- **新名称**: `dingtalk_bot_tool_python`
- **执行日期**: 2025-11-12

## 需要修改的文件清单

### 1. 扩展核心文件 (4 个文件)
- [ ] `manifest.json` - 第 3 行: name 字段
- [ ] `addon.py` - 第 14 行: @register_addon_as_extension 装饰器
- [ ] `extension.py` - 第 53 行: __init__ 方法默认参数
- [ ] `.ten/app/manifest-lock.json` - 第 6 行: name 字段

### 2. 文档文件 (2 个文件)
- [ ] `README.md` - 多处引用 (第 78, 93, 94, 122, 150, 151, 168 行)
- [ ] `README_CN.md` - 多处引用 (第 78, 93, 94, 122, 150, 151, 168 行)

### 3. 测试文件 (3 个文件)
- [ ] `tests/test_dingtalk_bot.py` - 第 84 行: set_test_mode_single 调用
- [ ] `tests/property.json` - 第 11, 12 行: name 和 addon 字段
- [ ] `.pytest_cache/v/cache/lastfailed` - 第 2 行: 测试路径

### 4. 示例项目配置 (2 个文件)
- [ ] `examples/text_voice-assistant/tenapp/manifest.json` - 第 146 行: 依赖路径
- [ ] `examples/text_voice-assistant/tenapp/property.json` - 第 99, 100, 138 行: name, addon, extension 字段

### 5. 目录重命名
- [ ] 将整个扩展目录从 `dingtalk_bot` 重命名为 `dingtalk_bot_tool_python`

## 修改详情

### 扩展名称字段变更
所有 `"name": "dingtalk_bot"` → `"name": "dingtalk_bot_tool_python"`
所有 `"addon": "dingtalk_bot"` → `"addon": "dingtalk_bot_tool_python"`
所有 `"extension": "dingtalk_bot"` → `"extension": "dingtalk_bot_tool_python"`

### 路径引用变更
`"path": "../../../ten_packages/extension/dingtalk_bot"` → `"path": "../../../ten_packages/extension/dingtalk_bot_tool_python"`

### 代码引用变更
- 装饰器: `@register_addon_as_extension("dingtalk_bot")` → `@register_addon_as_extension("dingtalk_bot_tool_python")`
- 构造函数: `def __init__(self, name: str = "dingtalk_bot")` → `def __init__(self, name: str = "dingtalk_bot_tool_python")`
- 测试: `tester.set_test_mode_single("dingtalk_bot", ...)` → `tester.set_test_mode_single("dingtalk_bot_tool_python", ...)`

## 执行顺序
1. 先修改扩展内部的所有文件
2. 修改示例项目的配置文件
3. 最后重命名目录

## 注意事项
⚠️ 目录重命名后，原有的文件路径将失效
⚠️ 需要重新运行 `task install` 安装依赖
⚠️ 可能需要清理缓存和锁文件后重新生成

## 验证检查
- [ ] 所有文件中的 `dingtalk_bot` 引用已更新
- [ ] manifest.json 中的 name 字段已更新
- [ ] 示例项目配置文件已更新
- [ ] 扩展可以正常加载和运行
- [ ] 工具注册功能正常