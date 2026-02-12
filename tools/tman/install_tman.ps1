# TEN Framework - TMAN Installation Script for Windows PowerShell
# Purpose: Install the latest version of tman on Windows with auto-detection of architecture

$ErrorActionPreference = "Stop"

# --- Color helpers ---
function Print-Info  { param($msg) Write-Host "[INFO] " -ForegroundColor Green -NoNewline; Write-Host $msg }
function Print-Warn  { param($msg) Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline; Write-Host $msg }
function Print-Error { param($msg) Write-Host "[ERROR] " -ForegroundColor Red -NoNewline; Write-Host $msg }

# --- Check existing installation ---
function Check-ExistingTman {
    $existing = Get-Command tman -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host ""
        Print-Warn "tman is already installed on this system"
        try { $ver = & tman --version 2>&1 } catch { $ver = "unknown" }
        Print-Info "Current version: $ver"
        Print-Info "Location: $($existing.Source)"
        Write-Host ""

        $reply = Read-Host "Do you want to reinstall/upgrade tman? [y/N]"
        if ($reply -notmatch '^[Yy]$') {
            Print-Info "Installation cancelled by user"
            Write-Host ""
            Print-Info "Quick tips:"
            Write-Host "  tman --version       # Check version"
            Write-Host "  tman --help          # Show help"
            Write-Host "  tman install         # Install project dependencies"
            Write-Host ""
            exit 0
        }
        Write-Host ""
        Print-Info "Proceeding with installation..."
        Write-Host ""
    }
}

# --- Detect architecture ---
function Detect-Arch {
    Print-Info "Detecting CPU architecture..."
    $machine = $env:PROCESSOR_ARCHITECTURE
    switch ($machine) {
        "AMD64"  { $script:ARCH = "x64" }
        "ARM64"  { $script:ARCH = "arm64" }
        default  {
            Print-Error "Unsupported architecture: $machine"
            Print-Info "Supported architectures: AMD64 (x64), ARM64"
            exit 1
        }
    }
    Print-Info "CPU architecture: $ARCH ($machine)"
}

function Detect-System {
    Print-Info "Detecting operating system..."
    Print-Info "Operating system: Windows"
    Detect-Arch
    $script:PLATFORM = "win-release-$ARCH"
    Print-Info "Target platform: $PLATFORM"
}

# --- Get latest version from GitHub ---
function Get-LatestVersion {
    Print-Info "Fetching latest version information..."
    $maxAttempts = 3
    $latestVersion = $null

    for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
        if ($attempt -gt 1) {
            Print-Info "Retry attempt $attempt of $maxAttempts..."
            Start-Sleep -Seconds 2
        }
        try {
            $response = Invoke-RestMethod -Uri "https://api.github.com/repos/TEN-framework/ten-framework/releases/latest" `
                -TimeoutSec 30 -ErrorAction Stop
            $latestVersion = $response.tag_name
            break
        } catch {
            Print-Warn "Attempt $attempt failed: $_"
        }
    }

    if (-not $latestVersion) {
        Print-Warn "Unable to fetch latest version automatically"
        Print-Info "Using default version: 0.11.53"
        $latestVersion = "0.11.53"
    } else {
        Print-Info "Latest version found: $latestVersion"
    }
    return $latestVersion
}

# --- Download tman ---
function Download-Tman {
    param([string]$Version)

    $downloadUrl = "https://github.com/TEN-framework/ten-framework/releases/download/$Version/tman-$PLATFORM.zip"
    $script:TmanTmpDir = Join-Path $env:TEMP "tman_install_$PID"

    Print-Info "Starting download of tman $Version..."
    Print-Info "Download URL: $downloadUrl"

    New-Item -ItemType Directory -Path $TmanTmpDir -Force | Out-Null
    $zipPath = Join-Path $TmanTmpDir "tman.zip"

    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -ErrorAction Stop
    } catch {
        Print-Error "Download failed"
        Print-Info "Possible reasons:"
        Print-Info "  1. Network connection issues"
        Print-Info "  2. Invalid version number"
        Print-Info "  3. Binary not available for your platform ($PLATFORM)"
        Write-Host ""
        Print-Info "Please visit: https://github.com/TEN-framework/ten-framework/releases"
        Remove-Item -Recurse -Force $TmanTmpDir -ErrorAction SilentlyContinue
        exit 1
    }
    Print-Info "Download completed"
}

# --- Extract and install ---
function Install-Tman {
    Print-Info "Extracting files..."
    $extractDir = Join-Path $TmanTmpDir "extracted"
    Expand-Archive -Path (Join-Path $TmanTmpDir "tman.zip") -DestinationPath $extractDir -Force

    # Find tman.exe
    $tmanBin = Get-ChildItem -Path $extractDir -Filter "tman.exe" -Recurse | Select-Object -First 1
    if (-not $tmanBin) {
        Print-Error "tman.exe not found in extracted files"
        Print-Info "Contents of extracted directory:"
        Get-ChildItem -Path $extractDir -Recurse | Format-Table Name, Length
        exit 1
    }
    Print-Info "Found tman: $($tmanBin.FullName)"

    # Install to user-local directory (no admin required)
    $installDir = Join-Path $env:LOCALAPPDATA "tman"
    if (-not (Test-Path $installDir)) {
        New-Item -ItemType Directory -Path $installDir -Force | Out-Null
    }

    Copy-Item -Path $tmanBin.FullName -Destination (Join-Path $installDir "tman.exe") -Force
    Print-Info "Installed tman to $installDir"

    # Add to user PATH if not already present
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$installDir*") {
        [Environment]::SetEnvironmentVariable("Path", "$installDir;$userPath", "User")
        $env:Path = "$installDir;$env:Path"
        Print-Info "Added $installDir to user PATH"
        Print-Warn "You may need to restart your terminal for PATH changes to take effect"
    } else {
        Print-Info "$installDir is already in PATH"
    }

    # Cleanup
    Print-Info "Cleaning up temporary files..."
    Remove-Item -Recurse -Force $TmanTmpDir -ErrorAction SilentlyContinue
}

# --- Verify installation ---
function Verify-Installation {
    Print-Info "Verifying installation..."
    $tmanCmd = Get-Command tman -ErrorAction SilentlyContinue
    if ($tmanCmd) {
        try { $ver = & tman --version 2>&1 } catch { $ver = "Unable to get version info" }
        Print-Info "tman installed successfully!"
        Print-Info "  Version: $ver"
        Print-Info "  Location: $($tmanCmd.Source)"
    } else {
        Print-Warn "tman not found in current PATH"
        Print-Info "Please restart your terminal, then run: tman --version"
    }
}

# --- Usage ---
function Show-Usage {
    Write-Host @"
Usage: .\install_tman.ps1 [VERSION]

Install TEN Framework TMAN tool with automatic architecture detection.

Arguments:
  VERSION    Optional. Specify a version to install (e.g., 0.11.53)
             If not provided, the latest version will be downloaded.

Examples:
  .\install_tman.ps1              # Install latest version
  .\install_tman.ps1 0.11.53      # Install specific version

Supported Platforms:
  - Windows x64 (AMD64)
  - Windows ARM64
"@
}

# --- Main ---
function Main {
    param([string]$RequestedVersion)

    if ($RequestedVersion -eq "-h" -or $RequestedVersion -eq "--help") {
        Show-Usage
        return
    }

    Write-Host "================================================"
    Write-Host "  TEN Framework - TMAN Installation Script"
    Write-Host "================================================"
    Write-Host ""

    Check-ExistingTman
    Detect-System
    Write-Host ""

    if ($RequestedVersion) {
        $version = $RequestedVersion
        Print-Info "Using specified version: $version"
    } else {
        $version = Get-LatestVersion
    }
    Write-Host ""

    Download-Tman -Version $version
    Write-Host ""

    Install-Tman
    Write-Host ""

    Verify-Installation

    Write-Host ""
    Write-Host "================================================"
    Print-Info "Installation completed successfully!"
    Write-Host "================================================"
    Write-Host ""
    Print-Info "Common commands:"
    Write-Host "  tman --version       # Check version"
    Write-Host "  tman --help          # Show help"
    Write-Host "  tman install         # Install project dependencies"
    Write-Host "  tman create <name>   # Create new project"
    Write-Host ""
}

Main $args[0]
