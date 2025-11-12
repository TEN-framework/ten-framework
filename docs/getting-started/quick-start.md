---
title: TEN Framework Quick Start Guide
_portal_target: getting-started/quick-start.md
---

## TEN Framework Quick Start Guide

> 🎯 **Goal**: Set up your development environment and run your first TEN app in 5 minutes

## System Requirements

**Supported Operating Systems**:

- Linux (x64)
- macOS Intel (x64)
- macOS Apple Silicon (arm64)

**Required Software**:

- Python 3.10
- Go 1.20+
- Node.js / npm (for managing JavaScript dependencies)

## Step 1: Check Your Environment

Before you begin, make sure the following software is installed on your system:

### Python 3.10

```bash
python3 --version
# Should display: Python 3.10.x
```

> 💡 **Recommendation**: It's recommended to use `pyenv` or `venv` to create a Python 3.10 virtual environment to avoid conflicts with your system Python version:
>
> ```bash
> # Create virtual environment using venv (example)
> python3.10 -m venv ~/ten-venv
> source ~/ten-venv/bin/activate
>
> # Or use pyenv to manage multiple Python versions (example)
> pyenv install 3.10.14
> pyenv local 3.10.14
> ```

### Go 1.20+

```bash
go version
# Should display: go version go1.20 or higher
```

### Node.js / npm

```bash
node --version
npm --version
# Ensure node and npm commands are available
```

> 💡 **Tip**: If any of the above is missing, please install the required version before continuing.

## Step 2: Install TEN Manager (tman)

TEN Manager (tman) is the command-line tool for TEN Framework, used to create projects, manage dependencies, and run applications.

**One-line Installation**:

```bash
curl -fsSL https://raw.githubusercontent.com/TEN-framework/ten-framework/main/tools/tman/install_tman.sh | bash
```

Or, if you've already cloned the repository:

```bash
cd ten-framework
bash tools/tman/install_tman.sh
```

**Verify Installation**:

```bash
tman --version
```

> 💡 **Tip**: If you see `tman: command not found`, make sure `/usr/local/bin` is in your PATH:
>
> ```bash
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc  # Linux
> echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc   # macOS
> source ~/.bashrc  # or source ~/.zshrc
> ```

## Step 3: Create and Run the Demo App

### 1. Create App

```bash
# Create a new transcriber_demo app
tman create app transcriber_demo --template transcriber_demo
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

```bash
# Create .env file
cat > .env << EOF
# Azure Speech Service Configuration
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region      # e.g., eastus
AZURE_STT_LANGUAGE=en-US                # Set according to your audio language or real-time recording language, e.g., zh-CN, ja-JP, ko-KR, etc.
EOF
```

> 💡 **Tip**: If you want to use other ASR extensions (such as OpenAI Whisper, Google Speech, etc.), you can download and replace them from the cloud store. Similarly, configure the corresponding API keys and environment variables in the `.env` file.

### 5. Run the App

```bash
tman run start
```

If everything is working correctly, you should see output similar to:

```text
[web_audio_control_go] Web server started on port 8080
[audio_file_player_python] AudioFilePlayerExtension on_start
```

### 6. Experience the Demo

Open your browser and visit:

```text
http://localhost:8080
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

### 3. Port 8080 Already in Use

**Problem**: Port conflict error when starting the app

**Solution**:

- Find the process using the port: `lsof -i :8080` (macOS/Linux)
- Kill the process: `kill -9 <PID>`
- Or modify the port number in the app configuration file (`transcriber_demo/ten_packages/extension/web_audio_control_go/property.json`)

### 4. Go Build Failed

**Problem**: Go module related errors during build

**Solution**:

```bash
# Clean Go module cache
go clean -modcache

# Rebuild
cd transcriber_demo
tman run build
```

### 5. Python Dependencies Installation Failed

**Problem**: pip installation timeout or failure

**Solution**: Use a mirror source (for users in China)

```bash
pip3 install --index-url https://pypi.tuna.tsinghua.edu.cn/simple -r requirements.txt
```

## Get Help

- **GitHub Issues**: <https://github.com/TEN-framework/ten-framework/issues>
- **Documentation**: <https://theten.ai/docs>
- **Contributing Guide**: [contributing.md](../code-of-conduct/contributing.md)
