---
title: TEN Framework 快速入门指南
_portal_target: getting-started/quick-start.cn.md
---

## TEN Framework 快速入门指南

> 🎯 **目标**：5分钟内搭建开发环境并运行第一个 TEN 应用

## 系统要求

**支持的操作系统**：

- Linux (x64)
- Linux (arm64)
- macOS Intel (x64)
- macOS Apple Silicon (arm64)
- Windows (x64)

**必需的软件环境**：

- Python 3.10
- Go 1.20+
- Node.js / npm（用于安装和管理 JavaScript 依赖）

> 💡 **Windows 用户注意**：推荐使用 PowerShell 执行本指南中的命令。部分命令在 CMD 中可能不兼容。

## 第一步：检查环境

在开始之前，请确保你的系统已安装以下软件：

### Python 3.10

```bash
python3 --version
# 应显示: Python 3.10.x
```

> 💡 **重要**：TEN Framework 目前仅支持 Python 3.10。推荐使用 `pyenv` 或 `venv` 创建 Python 虚拟环境：
>
> **Linux / macOS：**
>
> ```bash
> # 使用 pyenv 安装和管理 Python 3.10（推荐）
> pyenv install 3.10.18
> pyenv local 3.10.18
>
> # 或使用 venv 创建虚拟环境
> python3.10 -m venv ~/ten-venv
> source ~/ten-venv/bin/activate
> ```
>
> **Windows：**
>
> 从 <https://www.python.org/downloads/release/python-31011/> 底部表格中下载 Windows installer，启动后安装 Python 3.10。
>
> 注意，点击Install Now前务必勾选 "Add Python to PATH"
>
> ```bash
> # Windows平台需要配置python3命令：
>
> # 首先，确认python的安装路径：
> where.exe python
> # 输出示例: C:\Users\YourName\AppData\Local\Programs\Python\Python310\python.exe
>
> # 然后，以管理员身份打开 PowerShell，在python.exe同目录下创建symlink：
> New-Item -ItemType SymbolicLink -Path "C:\Users\YourName\AppData\Local\Programs\Python\Python310\python3.exe" -Target "C:\Users\YourName\AppData\Local\Programs\Python\Python310\python.exe"
> # 请将上面的路径替换为 where.exe python 的实际输出路径
>
> # 验证：
> python3 --version
> ```
>
> ```powershell
> # 推荐安装后使用 venv 创建虚拟环境，在该环境中工作
> py -3.10 -m venv $env:USERPROFILE\ten-venv
> # 激活环境
> & "$env:USERPROFILE\ten-venv\Scripts\Activate.ps1"
>
> # 若有权限错误，关闭终端/IDE，右键选择“以管理员身份运行”重新打开
> # 或者改变执行策略来允许ps1脚本执行
> Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
> ```

### Go 1.20+

```bash
go version
# 应显示: go version go1.20 或更高版本

$env:CGO_ENABLED = "1"
# Windows下需要显式启用CGO
```

### Node.js / npm

```bash
node --version
npm --version
# 确保 node 和 npm 命令可用
```

### GCC (MinGW)
💡仅在windows上需要安装
``` bash
# 首先确定winget存在。
winget --version

# 若不存在则需要从 <https://apps.microsoft.com/detail/9nblggh4nns1?hl=en-US&gl=US> 安装。
#（系统需要满足：Windows10 高于 1709 (Build 16299)，或者是Windows11）

#安装MinGW
winget install BrechtSanders.WinLibs.POSIX.MSVCRT

# 或查找后自行选择合适版本安装
winget search "mingw"

#检查安装
gcc --version
```

> 💡 **提示**：如果缺少上述环境，请先安装对应版本后再继续。

## 第二步：安装 TEN Manager (tman)

TEN Manager (tman) 是 TEN Framework 的命令行工具，用于创建项目、管理依赖和运行应用。

方式一：通过包管理器安装（推荐）

**Linux (Ubuntu/Debian):**

```bash
sudo add-apt-repository ppa:ten-framework/ten-framework
sudo apt update
sudo apt install tman
```

**macOS:**

```bash
brew install TEN-framework/ten-framework/tman
```

**Windows:**

```powershell
winget install TEN-framework.tman
```

方式二：通过安装脚本

**Linux / macOS：**

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.sh)
```

**Windows：**

```powershell
irm https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.ps1 | iex
```

方式三：如果你已经克隆了仓库，使用仓库内的安装脚本

```bash
cd ten-framework

# Linux/MacOS
bash tools/tman/install_tman.sh
# Windows
& "C:\sw\ten-framework\tools\tman\install_tman.ps1"
```

> 💡 **提示**：如果系统中已经安装了 tman，安装脚本会询问是否重新安装/升级，输入 `y` 继续安装，输入 `n` 取消。
>
> **非交互式安装**（适用于自动化脚本或 CI 环境，Windows不可用）：
>
> ```bash
> # 远程安装
> yes y | bash <(curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.sh)
>
> # 本地安装
> yes y | bash tools/tman/install_tman.sh
> ```

**验证安装**：

```bash
tman --version
```

>💡 **提示**：如果出现类似
>
>- `tman: command not found`（Linux/macOS）
>- `无法将"tman"项识别为 cmdlet、函数、脚本文件或可运行程序的名称`（Windows）
>
>
>   的提示，请确保 tman 所在目录在你的 PATH 中：
>
> **Linux / macOS：**
>
> ```bash
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc  # Linux
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc   # macOS
> source ~/.bashrc  # 或 source ~/.zshrc
> ```
>
> **Windows：**
>
> ```powershell
> # 检查 PATH 中是否包含 tman
> $env:Path -split ";" | Select-String "tman"
> # 如果不包含，请
> # 1. 通过 "系统属性 → 环境变量" 手动添加，然后重启终端以应用
> # 2. 运行以下命令来临时添加路径
> $env:PATH += ";$env:LOCALAPPDATA\tman"
> ```

## 第三步：创建并运行示例应用

### 1. 创建应用

```bash
# 创建一个新的 transcriber_demo 应用
tman install app transcriber_demo
cd transcriber_demo
```

### 2. 安装依赖

```bash
# 安装 TEN 包依赖
tman install

# 安装 Python 和 npm 包的依赖
tman run install_deps
```

> ⏱️ **预计时间**：1-2 分钟

### 3. 构建应用

```bash
tman run build
```

> ⏱️ **预计时间**：30 秒

### 4. 配置环境变量

在运行应用前，需要配置 ASR（语音识别）服务的密钥。当前示例使用 Azure ASR extension，你需要在 `transcriber_demo/.env` 文件中填写相关配置：

**Linux / macOS：**

```bash
# 创建 .env 文件
cat > .env << EOF
# Azure Speech Service 配置
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region      # 例如：eastus
AZURE_STT_LANGUAGE=en-US                # 根据你的音频语种或实时录音语种设置，如：zh-CN, ja-JP, ko-KR 等
EOF
```

**Windows（PowerShell）：**

```powershell
# 创建 .env 文件
@"
# Azure Speech Service 配置
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region
AZURE_STT_LANGUAGE=en-US
"@ | Out-File -Encoding utf8 .env
```

> 💡 **提示**：Windows 用户也可以直接用文本编辑器创建 `.env` 文件并填写上述配置。
>
> 💡 **提示**：如果你想使用其他 ASR extension（如 OpenAI Whisper、Google Speech 等），可以从云商店下载并替换，同样将相应的 API key 等环境变量配置在 `.env` 文件中。

### 5. 运行应用

```bash
tman run start
```

如果一切正常，你应该看到类似以下的输出：

```text
[web_audio_control_go] Web server started on port 8001
[audio_file_player_python] AudioFilePlayerExtension on_start
```

### 6. 体验 Demo

打开浏览器访问：

```text
http://localhost:8001
```

你应该能看到 Transcriber Demo 的 Web 界面，可以尝试：

- 点击麦克风按钮进行实时语音转录
- 上传音频文件进行转录
- 查看实时转录以及字幕结果

## 恭喜！🎉

你已经成功运行了第一个 TEN 应用！

### 了解应用架构

这个 `transcriber_demo` 应用展示了 TEN Framework 的多语言扩展能力，它由以下组件构成：

- **Go** - WebSocket 服务器扩展 (`web_audio_control_go`)
- **Python** - ASR 语音识别扩展 (`azure_asr_python`)
- **TypeScript** - VTT 字幕生成和音频录制扩展 (`vtt_nodejs`)

🎯 **你已经可以运行这些多语言插件了！**

### 下一步

现在你可以：

1. **从云商店探索和下载更多插件，设计和编排你的应用**

   ```bash
   tman designer  # 启动 TMAN Designer，在云商店中探索插件、下载插件并设计编排你的应用
   ```

2. **选择一个语言，开发自己的插件**
   - 支持 Go、Python、TypeScript/JavaScript、C++ 等多种语言
   - 查看 [TEN 扩展开发完整指南](https://theten.ai/cn/docs/ten_framework/development/how_to_develop_with_ext) 了解详情

## 进阶：开发和构建 C++ 插件

如果你想开发和使用 C++ 扩展，推荐安装 TEN 构建工具链（tgn）。以下是完整的步骤：

### 1. 安装 tgn 构建工具

tgn 是 TEN Framework 的 C/C++ 构建系统，基于 Google 的 GN。

方式一：一键安装（推荐）

**Linux / macOS：**

```bash
curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.sh | bash
```

**Windows：**

```powershell
irm https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.ps1 | iex
```

方式二：从克隆的仓库安装

**Linux / macOS：**

```bash
# 如果你已经克隆了 TEN Framework 仓库
cd ten-framework
bash tools/tgn/install_tgn.sh
```

**Windows：**

```powershell
# 如果你已经克隆了 TEN Framework 仓库
& ".\tools\tgn\install_tgn.ps1"
```

安装完成后，确认 tgn 已添加到 PATH：

**Linux / macOS：**

```bash
# 临时添加到当前会话
export PATH="/usr/local/ten_gn:$PATH"

# 或永久添加到 shell 配置（推荐）
echo 'export PATH="/usr/local/ten_gn:$PATH"' >> ~/.bashrc  # Linux
echo 'export PATH="/usr/local/ten_gn:$PATH"' >> ~/.zshrc   # macOS
source ~/.bashrc  # 或 source ~/.zshrc
```

**Windows：**

```powershell
# 安装脚本会自动添加到用户 PATH，如需手动添加：
$env:Path += ";$env:LOCALAPPDATA\ten_gn"
```

验证安装：

```bash
tgn --help
```

### 2. 安装 C++ 扩展

以 WebRTC VAD（语音活动检测）扩展为例，从云商店安装 C++ 扩展：

```bash
cd transcriber_demo
tman install extension webrtc_vad_cpp
```

> 💡 **提示**：`webrtc_vad_cpp` 是一个用 C++ 实现的语音活动检测扩展，可以在实时语音识别场景中筛选出语音部分。

### 3. 编译 C++ 扩展

安装 C++ 扩展后，需要重新构建应用以编译 C++ 代码为动态库：

```bash
tman run build
```

> ⏱️ **预计时间**：首次编译 C++ 扩展可能需要 1-3 分钟，具体取决于你的机器性能。

### 4. 运行带有 VAD 功能的应用

```bash
tman run start_with_vad
```

如果一切正常，你应该看到：

```text
[web_audio_control_go] Web server started on port 8001
[vad] WebRTC VAD initialized with mode 2
[audio_file_player_python] AudioFilePlayerExtension on_start
```

现在打开浏览器访问 `http://localhost:8001`，进入麦克风实时转写页面，你会看到经过vad后的silence状态变化，当silence状态为true时，表示当前音频中没有语音。

### C++ 开发环境要求

开发和编译 C++ 扩展需要安装 C++ 编译器（gcc 或 clang）：

**Linux:**

```bash
# Ubuntu/Debian
sudo apt-get install gcc g++

# 或使用 clang
sudo apt-get install clang
```

**macOS:**

```bash
# 安装 Xcode Command Line Tools (包含 clang)
xcode-select --install
```

**Windows:**

```powershell
# 方式一：安装 Visual Studio Build Tools（推荐）
# 从 https://visualstudio.microsoft.com/visual-cpp-build-tools/ 下载安装

# 方式二：使用 winget 安装
winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"

# 💡注意：
#  - 安装时选择 "使用 C++ 的桌面开发" 工作负载
#  - 且务必勾选“适用于Windows的C++ Clang工具”
```

验证编译器安装：

```bash
# 检查 gcc（Linux/macOS/MSYS2）
gcc --version
g++ --version

# 或检查 clang（Linux/macOS）
clang --version
```

**Windows（使用 MSVC）：**

```powershell
# 在 "Developer PowerShell for VS" 中执行
clang-cl --version
```

### 常见问题（C++ 扩展）

1. tgn 命令找不到

   确保已经执行安装脚本并将 tgn 添加到 PATH：

   ```bash
   export PATH="/usr/local/ten_gn:$PATH"
   ```

2. 编译失败：找不到编译器

   请参考上面的"C++ 开发环境要求"部分安装编译器。

### 了解更多

- [ten_gn 构建系统](https://github.com/TEN-framework/ten_gn) - TEN 的 C/C++ 构建工具
- [C++ 扩展开发指南](https://theten.ai/cn/docs/ten_framework/development/how_to_develop_with_ext) - 完整的 C++ 扩展开发文档

## 常见问题

### 1. macOS 上 Python 库加载失败

**问题**：运行应用时提示找不到 `libpython3.10.dylib`

**解决方案**：

```bash
export DYLD_LIBRARY_PATH=/usr/local/opt/python@3.10/Frameworks/Python.framework/Versions/3.10/lib:$DYLD_LIBRARY_PATH
```

建议将这行添加到 `~/.zshrc` 或 `~/.bash_profile` 中。

### 2. tman 下载失败或速度很慢

**问题**：网络连接 GitHub 受限

**解决方案**：

- 手动下载：访问 [Releases 页面](https://github.com/TEN-framework/ten-framework/releases) 下载对应平台的 `tman` 二进制文件

### 3. 端口 8001 已被占用

**问题**：启动时提示端口冲突

**解决方案**：

- 查找占用端口的进程：
  - Linux/macOS：`lsof -i :8001`
  - Windows：`netstat -ano | findstr :8001`
- 杀掉该进程：
  - Linux/macOS：`kill -9 <PID>`
  - Windows：`taskkill /PID <PID> /F`
- 或修改应用配置文件（`transcriber_demo/ten_packages/extension/web_audio_control_go/property.json`）中的端口号

### 4. Go build 失败

**问题**：构建时提示 Go module 相关错误

**解决方案**：

```bash
# 设置代理
# Linux/macOS
export GOPROXY=https://goproxy.cn,direct
# Windows PowerShell
$env:GOPROXY = "https://goproxy.cn,direct"

# 清理 Go module 缓存
go clean -modcache

# 重新安装依赖
cd transcriber_demo
tman run build
```

### 5. Python 依赖安装失败

**问题**：pip 安装超时或失败

**解决方案**：使用国内镜像源

```bash
pip3 install --index-url https://pypi.tuna.tsinghua.edu.cn/simple -r requirements.txt
```

### 6. Azure语音服务报错

**问题**：Azure语音服务相关错误，比如认证失败

**解决方案**：检查.env文件中的配置是否正确，确保 AZURE_STT_KEY 和 AZURE_STT_REGION 填写无误

### 7. Windows下权限问题

**问题**：Windows下在访问文件时报PermissionError

**解决方案**：右键点击PowerShell，选择“以管理员身份运行”

## 获取帮助

- **GitHub Issues**：<https://github.com/TEN-framework/ten-framework/issues>
- **文档**：<https://theten.ai/cn/docs>
- **贡献指南**：[contributing.md](../code-of-conduct/contributing.md)
