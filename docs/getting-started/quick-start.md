---
title: TEN Framework Quick Start Guide
_portal_target: getting-started/quick-start.md
---

## TEN Framework Quick Start Guide

> 🎯 **Goal**: Set up your development environment and run your first TEN app in 5 minutes

## System Requirements

**Supported Operating Systems**:

- Linux (x64)
- Linux (arm64)
- macOS Intel (x64)
- macOS Apple Silicon (arm64)
- Windows (x64)

**Required Software**:

- Python 3.10
- Go 1.20+
- Node.js / npm (for managing JavaScript dependencies)

> 💡 **Windows users**: It's recommended to use PowerShell to run the commands in this guide. Some commands may not be compatible with CMD.

## Step 1: Check Your Environment

Before you begin, make sure the following software is installed on your system:

### Python 3.10

```bash
python3 --version
# Should display: Python 3.10.x
```

> 💡 **Important**: TEN Framework currently only supports Python 3.10. It's recommended to use `pyenv` or `venv` to manage your Python environment:
>
> **Linux / macOS:**
>
> ```bash
> # Install and manage Python 3.10 using pyenv (recommended)
> pyenv install 3.10.14
> pyenv local 3.10.14
>
> # Or create virtual environment using venv
> python3.10 -m venv ~/ten-venv
> source ~/ten-venv/bin/activate
> ```
>
> **Windows:**
>
> Download the Windows installer from the bottom table at <https://www.python.org/downloads/release/python-31011/> and run it to install Python 3.10.
>
> Make sure to check "Add Python to PATH" before clicking Install Now.
>
> ```bash
> # On Windows, configure the python3 command:
>
> # First, find where python is installed:
> where.exe python
> # Example output: C:\Users\YourName\AppData\Local\Programs\Python\Python310\python.exe
>
> # Then, open PowerShell as Administrator and create a symlink in the same directory:
> New-Item -ItemType SymbolicLink -Path "C:\Users\YourName\AppData\Local\Programs\Python\Python310\python3.exe" -Target "C:\Users\YourName\AppData\Local\Programs\Python\Python310\python.exe"
> # Replace the paths above with the actual output from where.exe python
>
> # Verify:
> python3 --version
> ```
>
> ```powershell
> # It's recommended to create a venv after installation
> py -3.10 -m venv $env:USERPROFILE\ten-venv
> # Activate the environment
> & "$env:USERPROFILE\ten-venv\Scripts\Activate.ps1"
>
> # If you encounter a permission error, close the terminal/IDE, right-click and select "Run as Administrator" to reopen
> # Or change the execution policy to allow ps1 scripts
> Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
> ```

### Go 1.20+

```bash
go version
# Should display: go version go1.20 or higher

$env:CGO_ENABLED = "1"
# CGO must be explicitly enabled on Windows
```

### Node.js / npm

```bash
node --version
npm --version
# Ensure node and npm commands are available
```

### GCC (MinGW)
💡 Only required on Windows
```bash
# First, make sure winget is available.
winget --version

# If not, install it from <https://apps.microsoft.com/detail/9nblggh4nns1?hl=en-US&gl=US>
# (Requires Windows 10 version 1709 (Build 16299) or later, or Windows 11)

# Install MinGW
winget install BrechtSanders.WinLibs.POSIX.MSVCRT

# Or search and choose a suitable version to install
winget search "mingw"

# Verify installation
gcc --version
```

> 💡 **Tip**: If any of the above is missing, please install the required version before continuing.

## Step 2: Install TEN Manager (tman)

TEN Manager (tman) is the command-line tool for TEN Framework, used to create projects, manage dependencies, and run applications.

Option 1: Install via Package Manager (Recommended)

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

Option 2: Install via Script

**Linux / macOS:**

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.sh)
```

**Windows:**

```powershell
irm https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.ps1 | iex
```

Option 3: Install from cloned repository

```bash
cd ten-framework

# Linux/macOS
bash tools/tman/install_tman.sh
# Windows
& "C:\sw\ten-framework\tools\tman\install_tman.ps1"
```

> 💡 **Note**: If tman is already installed on your system, the installation script will ask whether you want to reinstall/upgrade it. Press `y` to continue or `n` to cancel.
>
> **Non-interactive Installation** (for automation scripts or CI environments, not available on Windows):
>
> ```bash
> # Remote installation
> yes y | bash <(curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.sh)
>
> # Local installation
> yes y | bash tools/tman/install_tman.sh
> ```

**Verify Installation**:

```bash
tman --version
```

> 💡 **Tip**: If you see `tman: command not found` (Linux/macOS) or a similar unrecognized command error (Windows), make sure the tman directory is in your PATH:
>
> **Linux / macOS:**
>
> ```bash
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc  # Linux
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc   # macOS
> source ~/.bashrc  # or source ~/.zshrc
> ```
>
> **Windows:**
>
> ```powershell
> # Check if tman is in PATH
> $env:Path -split ";" | Select-String "tman"
> # If not, either:
> # 1. Add it manually via "System Properties → Environment Variables", then restart your terminal
> # 2. Or temporarily add the path with:
> $env:PATH += ";$env:LOCALAPPDATA\tman"
> ```

## Step 3: Create and Run the Demo App

### 1. Create App

```bash
# Create a new transcriber_demo app
tman install app transcriber_demo
cd transcriber_demo
```

### 2. Install Dependencies

```bash
# Install TEN package dependencies
tman install

# Install Python and npm package dependencies
tman run install_deps
```

> ⏱️ **Estimated Time**: 1-2 minutes

### 3. Build the App

```bash
tman run build
```

> ⏱️ **Estimated Time**: 30 seconds

### 4. Configure Environment Variables

Before running the app, you need to configure the ASR (Automatic Speech Recognition) service credentials. The current example uses Azure ASR extension. You need to fill in the configuration in the `transcriber_demo/.env` file:

**Linux / macOS:**

```bash
# Create .env file
cat > .env << EOF
# Azure Speech Service Configuration
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region      # e.g., eastus
AZURE_STT_LANGUAGE=en-US                # Set according to your audio language or real-time recording language, e.g., zh-CN, ja-JP, ko-KR, etc.
EOF
```

**Windows (PowerShell):**

```powershell
# Create .env file
@"
# Azure Speech Service Configuration
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region
AZURE_STT_LANGUAGE=en-US
"@ | Out-File -Encoding utf8 .env
```

> 💡 **Tip**: Windows users can also create the `.env` file directly with a text editor and fill in the configuration above.
>
> 💡 **Tip**: If you want to use other ASR extensions (such as OpenAI Whisper, Google Speech, etc.), you can download and replace them from the cloud store. Similarly, configure the corresponding API keys and environment variables in the `.env` file.

### 5. Run the App

```bash
tman run start
```

If everything is working correctly, you should see output similar to:

```text
[web_audio_control_go] Web server started on port 8001
[audio_file_player_python] AudioFilePlayerExtension on_start
```

### 6. Experience the Demo

Open your browser and visit:

```text
http://localhost:8001
```

You should see the Transcriber Demo web interface. Try:

- Click the microphone button for real-time voice transcription
- Upload an audio file for transcription
- View real-time transcription and subtitle results

## Congratulations! 🎉

You've successfully run your first TEN application!

### Understanding the App Architecture

This `transcriber_demo` app showcases TEN Framework's multi-language extension capabilities, consisting of:

- **Go** - WebSocket server extension (`web_audio_control_go`)
- **Python** - ASR speech recognition extension (`azure_asr_python`)
- **TypeScript** - VTT subtitle generation and audio recording extension (`vtt_nodejs`)

🎯 **You can now run extensions in multiple languages!**

### Next Steps

Now you can:

1. **Explore and download more extensions from the cloud store, design and orchestrate your app**

   ```bash
   tman designer  # Launch TMAN Designer to explore extensions, download them, and design your app
   ```

2. **Choose a language and develop your own extension**
   - Supports Go, Python, TypeScript/JavaScript, C++, and more
   - Check out the [TEN Extension Development Guide](https://theten.ai/docs/ten_framework/development/how_to_develop_with_ext) for details

## Advanced: Developing and Building C++ Extensions

If you want to develop and use C++ extensions, you'll need to install the TEN build toolchain (tgn). Here's the complete process:

### 1. Install tgn Build Tool

tgn is TEN Framework's C/C++ build system, based on Google's GN.

Option 1: One-line Installation (Recommended)

**Linux / macOS:**

```bash
curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.sh | bash
```

**Windows:**

```powershell
irm https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tgn/install_tgn.ps1 | iex
```

Option 2: Install from Cloned Repository

**Linux / macOS:**

```bash
# If you've already cloned the TEN Framework repository
cd ten-framework
bash tools/tgn/install_tgn.sh
```

**Windows:**

```powershell
# If you've already cloned the TEN Framework repository
& ".\tools\tgn\install_tgn.ps1"
```

After installation, ensure tgn is added to PATH:

**Linux / macOS:**

```bash
# Temporarily add to current session
export PATH="/usr/local/ten_gn:$PATH"

# Or permanently add to shell configuration (recommended)
echo 'export PATH="/usr/local/ten_gn:$PATH"' >> ~/.bashrc  # Linux
echo 'export PATH="/usr/local/ten_gn:$PATH"' >> ~/.zshrc   # macOS
source ~/.bashrc  # or source ~/.zshrc
```

**Windows:**

```powershell
# The install script adds to user PATH automatically. To add manually:
$env:Path += ";$env:LOCALAPPDATA\ten_gn"
```

Verify installation:

```bash
tgn --help
```

### 2. Install C++ Extension

Using WebRTC VAD (Voice Activity Detection) extension as an example, install a C++ extension from the cloud store:

```bash
cd transcriber_demo
tman install extension webrtc_vad_cpp
```

> 💡 **Note**: `webrtc_vad_cpp` is a voice activity detection extension implemented in C++. It can filter out non-speech segments in real-time speech recognition scenarios.

### 3. Compile C++ Extension

After installing the C++ extension, rebuild the app to compile the C++ code into a dynamic library:

```bash
tman run build
```

> ⏱️ **Expected Time**: First-time compilation of C++ extensions may take 1-3 minutes, depending on your machine's performance.

### 4. Run App with VAD Functionality

```bash
tman run start_with_vad
```

If everything works correctly, you should see:

```text
[web_audio_control_go] Web server started on port 8001
[vad] WebRTC VAD initialized with mode 2
[audio_file_player_python] AudioFilePlayerExtension on_start
```

Now open your browser at `http://localhost:8001` and navigate to the microphone real-time transcription page. You'll see the silence state changes after VAD processing. When the silence state is true, it indicates there is no speech in the current audio.

### C++ Development Environment Requirements

Developing and compiling C++ extensions requires installing a C++ compiler (gcc or clang):

**Linux:**

```bash
# Ubuntu/Debian
sudo apt-get install gcc g++

# Or use clang
sudo apt-get install clang
```

**macOS:**

```bash
# Install Xcode Command Line Tools (includes clang)
xcode-select --install
```

**Windows:**

```powershell
# Option 1: Install Visual Studio Build Tools (recommended)
# Download from https://visualstudio.microsoft.com/visual-cpp-build-tools/

# Option 2: Install via winget
winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"

# 💡 Note:
#  - Select the "Desktop development with C++" workload during installation
#  - Make sure to check "C++ Clang tools for Windows"
```

Verify compiler installation:

```bash
# Check gcc (Linux/macOS/MSYS2)
gcc --version
g++ --version

# Or check clang (Linux/macOS)
clang --version
```

**Windows (MSVC):**

```powershell
# Run in "Developer PowerShell for VS"
clang-cl --version
```

### Troubleshooting (C++ Extensions)

1. tgn command not found

   Ensure you've run the installation script and added tgn to PATH:

   ```bash
   export PATH="/usr/local/ten_gn:$PATH"
   ```

2. Compilation failed: Compiler not found

   Please refer to the "C++ Development Environment Requirements" section above to install the compiler.

### Learn More

- [ten_gn Build System](https://github.com/TEN-framework/ten_gn) - TEN's C/C++ build tool
- [C++ Extension Development Guide](https://theten.ai/docs/ten_framework/development/how_to_develop_with_ext) - Complete C++ extension development documentation

## Troubleshooting

### 1. Python Library Loading Failure on macOS

**Problem**: Error indicating `libpython3.10.dylib` cannot be found when running the app

**Solution**:

```bash
export DYLD_LIBRARY_PATH=/usr/local/opt/python@3.10/Frameworks/Python.framework/Versions/3.10/lib:$DYLD_LIBRARY_PATH
```

Consider adding this line to your `~/.zshrc` or `~/.bash_profile`.

### 2. tman Download Failed or Slow

**Problem**: Network connection to GitHub is restricted

**Solution**:

- Manual download: Visit the [Releases page](https://github.com/TEN-framework/ten-framework/releases) to download the `tman` binary for your platform

### 3. Port 8001 Already in Use

**Problem**: Port conflict error when starting the app

**Solution**:

- Find the process using the port:
  - Linux/macOS: `lsof -i :8001`
  - Windows: `netstat -ano | findstr :8001`
- Kill the process:
  - Linux/macOS: `kill -9 <PID>`
  - Windows: `taskkill /PID <PID> /F`
- Or modify the port number in the app configuration file (`transcriber_demo/ten_packages/extension/web_audio_control_go/property.json`)

### 4. Go Build Failed

**Problem**: Go module related errors during build

**Solution**:

```
# Set proxy
# Linux/macOS
export GOPROXY=https://goproxy.cn,direct
# Windows PowerShell
$env:GOPROXY = "https://goproxy.cn,direct"

# Clean Go module cache
go clean -modcache

# Reinstall dependencies
cd transcriber_demo
tman run build
```

### 5. Python Dependencies Installation Failed

**Problem**: pip installation timeout or failure

**Solution**: Use a mirror source (for users in China)

```bash
pip3 install --index-url https://pypi.tuna.tsinghua.edu.cn/simple -r requirements.txt
```

### 6. Azure Speech Service Error

**Problem**: Azure Speech Service related errors, such as authentication failure

**Solution**: Check that the configuration in your `.env` file is correct, and ensure `AZURE_STT_KEY` and `AZURE_STT_REGION` are filled in accurately

### 7. Permission Error on Windows

**Problem**: PermissionError when accessing files on Windows

**Solution**: Right-click PowerShell and select "Run as Administrator"

## Get Help

- **GitHub Issues**: <https://github.com/TEN-framework/ten-framework/issues>
- **Documentation**: <https://theten.ai/docs>
- **Contributing Guide**: [contributing.md](../code-of-conduct/contributing.md)
