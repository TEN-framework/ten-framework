# TEN Framework - TGN Installation Script for Windows PowerShell
# Purpose: Install ten_gn build system on Windows
# Reference: https://github.com/TEN-framework/ten_gn

$ErrorActionPreference = "Stop"

# --- Configuration ---
$TGN_VERSION = if ($env:TGN_VERSION) { $env:TGN_VERSION } else { "main" }
$TGN_REPO = "https://github.com/TEN-framework/ten_gn.git"
$INSTALL_DIR = if ($env:TGN_INSTALL_DIR) { $env:TGN_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA "ten_gn" }

# --- Color helpers ---
function Print-Info  { param($msg) Write-Host "[INFO] " -ForegroundColor Green -NoNewline; Write-Host $msg }
function Print-Warn  { param($msg) Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline; Write-Host $msg }
function Print-Error { param($msg) Write-Host "[ERROR] " -ForegroundColor Red -NoNewline; Write-Host $msg }

# --- Check existing installation ---
function Check-ExistingTgn {
    if (Test-Path $INSTALL_DIR) {
        Write-Host ""
        Print-Warn "tgn is already installed at $INSTALL_DIR"

        if (Test-Path (Join-Path $INSTALL_DIR ".git")) {
            try {
                $currentVersion = & git -C $INSTALL_DIR describe --tags 2>$null
                if (-not $currentVersion) {
                    $currentVersion = & git -C $INSTALL_DIR rev-parse --short HEAD 2>$null
                }
            } catch {
                $currentVersion = "unknown"
            }
            if (-not $currentVersion) { $currentVersion = "unknown" }
            Print-Info "Current version: $currentVersion"
        }

        Write-Host ""

        # In CI environment, automatically proceed
        if ($env:CI -or $env:GITHUB_ACTIONS) {
            Print-Info "CI environment detected, proceeding with reinstallation..."
            Remove-Item -Recurse -Force $INSTALL_DIR
            return
        }

        $reply = Read-Host "Do you want to reinstall/upgrade tgn? [y/N]"
        if ($reply -notmatch '^[Yy]$') {
            Print-Info "Installation cancelled by user"
            Write-Host ""
            Print-Info "tgn is already available at: $INSTALL_DIR"
            Print-Info "  Ensure it is in your PATH: `$env:Path += `";$INSTALL_DIR`""
            Write-Host ""
            exit 0
        }

        Write-Host ""
        Print-Info "Proceeding with installation..."
        Remove-Item -Recurse -Force $INSTALL_DIR
        Write-Host ""
    }
}

# --- Check prerequisites ---
function Check-Prerequisites {
    Print-Info "Checking prerequisites..."

    # Check for git
    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $gitCmd) {
        Print-Error "git is not installed"
        Print-Info "Please install git first:"
        Print-Info "  winget install Git.Git"
        Print-Info "  or download from https://git-scm.com/download/win"
        exit 1
    }

    # Check for python
    $pythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCmd) {
        $pythonCmd = Get-Command python3 -ErrorAction SilentlyContinue
    }
    if (-not $pythonCmd) {
        Print-Error "python is not installed"
        Print-Info "Please install Python 3.10+ first"
        exit 1
    }

    $gitVersion = & git --version 2>&1
    $pythonVersion = & $pythonCmd.Source --version 2>&1
    Print-Info "git: $gitVersion"
    Print-Info "python: $pythonVersion"
}

# --- Install tgn ---
function Install-Tgn {
    Print-Info "Installing tgn (TEN build system)..."
    Print-Info "Repository: $TGN_REPO"
    Print-Info "Version: $TGN_VERSION"
    Print-Info "Install directory: $INSTALL_DIR"
    Write-Host ""

    # Create parent directory if needed
    $parentDir = Split-Path $INSTALL_DIR -Parent
    if (-not (Test-Path $parentDir)) {
        Print-Info "Creating directory: $parentDir"
        New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
    }

    # Clone the repository
    Print-Info "Cloning ten_gn repository..."
    & git clone $TGN_REPO $INSTALL_DIR
    if ($LASTEXITCODE -ne 0) {
        Print-Error "Failed to clone repository"
        exit 1
    }

    Push-Location $INSTALL_DIR
    try {
        & git checkout $TGN_VERSION
        if ($LASTEXITCODE -ne 0) {
            Print-Error "Failed to checkout version: $TGN_VERSION"
            exit 1
        }
    } finally {
        Pop-Location
    }

    Write-Host ""
    Print-Info "tgn installed successfully!"
}

# --- Configure PATH ---
function Configure-Path {
    Print-Info "Configuring PATH..."

    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -like "*$INSTALL_DIR*") {
        Print-Info "PATH already contains $INSTALL_DIR"
        return
    }

    # In CI environment, skip modifying PATH permanently
    if ($env:CI -or $env:GITHUB_ACTIONS) {
        Print-Info "CI environment detected, adding to session PATH only"
        $env:Path = "$INSTALL_DIR;$env:Path"
        return
    }

    Write-Host ""
    Print-Info "Add tgn to PATH by running:"
    Write-Host ""
    Write-Host "    `$env:Path += `";$INSTALL_DIR`""
    Write-Host ""

    $reply = Read-Host "Would you like to add it to your user PATH permanently? [Y/n]"
    if ($reply -notmatch '^[Nn]$') {
        [Environment]::SetEnvironmentVariable("Path", "$INSTALL_DIR;$userPath", "User")
        $env:Path = "$INSTALL_DIR;$env:Path"
        Print-Info "Added $INSTALL_DIR to user PATH"
        Print-Warn "You may need to restart your terminal for PATH changes to take effect"
    }
}

# --- Verify installation ---
function Verify-Installation {
    Print-Info "Verifying installation..."

    # Ensure PATH includes install dir for this session
    if ($env:Path -notlike "*$INSTALL_DIR*") {
        $env:Path = "$INSTALL_DIR;$env:Path"
    }

    $tgnScript = Join-Path $INSTALL_DIR "tgn"
    $tgnBat = Join-Path $INSTALL_DIR "tgn.bat"
    $tgnPy = Join-Path $INSTALL_DIR "tgn.py"

    if ((Test-Path $tgnScript) -or (Test-Path $tgnBat) -or (Test-Path $tgnPy)) {
        Print-Info "tgn files found at $INSTALL_DIR"
    } else {
        Print-Warn "tgn entry point not found in $INSTALL_DIR"
        Print-Info "Please check the repository contents"
    }

    Write-Host ""
    Write-Host "================================================"
    Print-Info "tgn installation completed successfully!"
    Write-Host "================================================"
    Write-Host ""
    Print-Info "Installation directory: $INSTALL_DIR"
    Print-Info "Version: $TGN_VERSION"
    Write-Host ""
    Print-Info "Quick start:"
    Write-Host "  tgn gen win x64 release           # Generate build files"
    Write-Host "  tgn build win x64 release         # Build project"
    Write-Host ""
}

# --- Main ---
function Main {
    Write-Host ""
    Write-Host "================================================"
    Write-Host "  TEN Framework - TGN Installation Script"
    Write-Host "================================================"
    Write-Host ""

    Check-ExistingTgn
    Check-Prerequisites
    Install-Tgn
    Configure-Path
    Verify-Installation
}

# Run main function
Main
