<#
.SYNOPSIS
    Submit tman package to Windows Package Manager (winget) repository.

.DESCRIPTION
    This script automates the process of submitting tman to the microsoft/winget-pkgs
    repository. It can be used for manual submissions or debugging the automated workflow.

    The script performs the following steps:
    1. Downloads the Windows release asset from GitHub
    2. Calculates SHA256 checksum
    3. Generates winget manifest files
    4. Forks and clones microsoft/winget-pkgs (if needed)
    5. Creates a new branch with the manifests
    6. Submits a Pull Request to microsoft/winget-pkgs

.PARAMETER Version
    The version tag to submit (e.g., "0.11.35" or "v0.11.35").
    If not provided, will use the latest release.

.PARAMETER GitHubToken
    GitHub Personal Access Token with repo and workflow permissions.
    Required for forking and creating PRs.
    Can also be set via GITHUB_TOKEN environment variable.

.PARAMETER Repository
    The GitHub repository in "owner/repo" format.
    Default: "TEN-framework/ten-framework"

.PARAMETER DryRun
    If specified, will prepare manifests but not submit the PR.
    Useful for testing and verification.

.EXAMPLE
    .\submit-to-winget.ps1 -Version "0.11.35" -GitHubToken "ghp_xxxxx"

    Submit version 0.11.35 to winget-pkgs.

.EXAMPLE
    .\submit-to-winget.ps1 -DryRun

    Prepare manifests for the latest version without submitting PR.

.NOTES
    Author: TEN Framework Team
    Requires: PowerShell 7+, Git, GitHub CLI (gh)
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string]$Version,

    [Parameter(Mandatory=$false)]
    [string]$GitHubToken = $env:GITHUB_TOKEN,

    [Parameter(Mandatory=$false)]
    [string]$Repository = "TEN-framework/ten-framework",

    [Parameter(Mandatory=$false)]
    [switch]$DryRun
)

# ==============================================================================
# Configuration and Constants
# ==============================================================================

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Colors for output
$ColorInfo = "Cyan"
$ColorSuccess = "Green"
$ColorWarning = "Yellow"
$ColorError = "Red"

# ==============================================================================
# Helper Functions
# ==============================================================================

function Write-Info {
    param([string]$Message)
    Write-Host "ℹ️  $Message" -ForegroundColor $ColorInfo
}

function Write-Success {
    param([string]$Message)
    Write-Host "✅ $Message" -ForegroundColor $ColorSuccess
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠️  $Message" -ForegroundColor $ColorWarning
}

function Write-Error {
    param([string]$Message)
    Write-Host "❌ $Message" -ForegroundColor $ColorError
}

function Test-CommandExists {
    param([string]$Command)
    return $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

# ==============================================================================
# Prerequisite Checks
# ==============================================================================

Write-Info "Checking prerequisites..."

# Skip checks in DryRun mode (only need basic PowerShell for manifest generation)
if (-not $DryRun) {
    # Check if Git is installed
    if (-not (Test-CommandExists "git")) {
        Write-Error "Git is not installed. Please install Git and try again."
        exit 1
    }

    # Check if GitHub CLI is installed
    if (-not (Test-CommandExists "gh")) {
        Write-Error "GitHub CLI (gh) is not installed."
        Write-Info "Install from: https://cli.github.com/"
        exit 1
    }

    # Check if GitHub token is provided
    if ([string]::IsNullOrEmpty($GitHubToken)) {
        Write-Error "GitHub token is required."
        Write-Info "Provide via -GitHubToken parameter or GITHUB_TOKEN environment variable."
        exit 1
    }
}

Write-Success "All prerequisites met"

# ==============================================================================
# Step 1: Determine Version
# ==============================================================================

Write-Info "Determining version to submit..."

if ([string]::IsNullOrEmpty($Version)) {
    # Fetch latest release from GitHub API
    Write-Info "No version specified, fetching latest release..."
    $apiUrl = "https://api.github.com/repos/$Repository/releases/latest"
    $response = Invoke-RestMethod -Uri $apiUrl
    $Version = $response.tag_name
    Write-Info "Latest release: $Version"
}

# Remove 'v' prefix if present
$VersionClean = $Version -replace '^v', ''
Write-Success "Version to submit: $VersionClean (tag: $Version)"

# ==============================================================================
# Step 2: Download Release Asset
# ==============================================================================

Write-Info "Downloading Windows release asset..."

$assetFileName = "tman-win-release-x64.zip"
$assetUrl = "https://github.com/$Repository/releases/download/$Version/$assetFileName"

Write-Info "Asset URL: $assetUrl"

try {
    Invoke-WebRequest -Uri $assetUrl -OutFile $assetFileName
    $fileSize = (Get-Item $assetFileName).Length
    Write-Success "Downloaded $assetFileName ($([math]::Round($fileSize/1MB, 2)) MB)"
} catch {
    Write-Error "Failed to download release asset: $_"
    Write-Info "Make sure the release exists and contains the Windows x64 zip file."
    exit 1
}

# ==============================================================================
# Step 3: Calculate SHA256 Checksum
# ==============================================================================

Write-Info "Calculating SHA256 checksum..."

$sha256 = (Get-FileHash -Path $assetFileName -Algorithm SHA256).Hash.ToLower()
Write-Success "SHA256: $sha256"

# ==============================================================================
# Step 4: Generate Manifest Files (from templates if available)
# ==============================================================================

Write-Info "Generating winget manifest files..."

$manifestDir = "winget-manifests"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null

# Check if we're running from within the repo (templates available)
$scriptDir = Split-Path -Parent $PSCommandPath
$templatesAvailable = Test-Path "$scriptDir\manifest.version.yaml.template"

if ($templatesAvailable) {
    Write-Info "Using templates from: $scriptDir"

    # ========================================================================
    # Generate from templates (workflow mode)
    # ========================================================================

    # Version Manifest
    $versionTemplate = Get-Content "$scriptDir\manifest.version.yaml.template" -Raw
    $versionContent = ($versionTemplate -split "`n" | Where-Object {
        $_.Trim() -notmatch '^#' -and $_.Trim() -ne ''
    }) -join "`n"
    $versionContent = $versionContent -replace '__VERSION__', $VersionClean
    $versionContent | Out-File -FilePath "$manifestDir\TENFramework.tman.yaml" -Encoding utf8 -NoNewline

    # Installer Manifest
    $installerTemplate = Get-Content "$scriptDir\manifest.installer.yaml.template" -Raw
    $installerContent = ($installerTemplate -split "`n" | Where-Object {
        $_.Trim() -notmatch '^#' -and $_.Trim() -ne ''
    }) -join "`n"
    $installerContent = $installerContent -replace '__VERSION__', $Version
    $installerContent = $installerContent -replace '__WIN_X64_SHA256__', $sha256
    $installerContent | Out-File -FilePath "$manifestDir\TENFramework.tman.installer.yaml" -Encoding utf8 -NoNewline

    # Locale Manifests (multiple languages)
    $locales = @("en-US", "zh-CN", "zh-TW", "ja-JP", "ko-KR")
    foreach ($locale in $locales) {
        $localeTemplateFile = "$scriptDir\manifest.locale.$locale.yaml.template"
        if (Test-Path $localeTemplateFile) {
            Write-Info "  - Generating $locale locale from template..."
            $localeTemplate = Get-Content $localeTemplateFile -Raw
            $localeContent = ($localeTemplate -split "`n" | Where-Object {
                $_.Trim() -notmatch '^#' -and $_.Trim() -ne ''
            }) -join "`n"
            $localeContent = $localeContent -replace '__VERSION__', $VersionClean
            $localeContent | Out-File -FilePath "$manifestDir\TENFramework.tman.locale.$locale.yaml" -Encoding utf8 -NoNewline
        }
    }
} else {
    Write-Info "Templates not found, generating inline (standalone mode)"

    # ========================================================================
    # Generate inline (standalone mode - backwards compatibility)
    # ========================================================================

    # Version Manifest
    $versionManifest = @"
PackageIdentifier: TENFramework.tman
PackageVersion: $VersionClean
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
"@

    $versionManifest | Out-File -FilePath "$manifestDir\TENFramework.tman.yaml" -Encoding utf8 -NoNewline

    # Installer Manifest
    $installerManifest = @"
PackageIdentifier: TENFramework.tman
PackageVersion: $VersionClean
InstallerType: zip
Installers:
- Architecture: x64
  InstallerUrl: $assetUrl
  InstallerSha256: $sha256
  NestedInstallerType: portable
  NestedInstallerFiles:
  - RelativeFilePath: ten_manager\bin\tman.exe
    PortableCommandAlias: tman
ManifestType: installer
ManifestVersion: 1.6.0
"@

    $installerManifest | Out-File -FilePath "$manifestDir\TENFramework.tman.installer.yaml" -Encoding utf8 -NoNewline

    # Locale Manifest (en-US only in standalone mode)
    $localeManifest = @"
PackageIdentifier: TENFramework.tman
PackageVersion: $VersionClean
PackageLocale: en-US
Publisher: TEN Framework Team
PublisherUrl: https://github.com/TEN-framework
PublisherSupportUrl: https://github.com/TEN-framework/ten-framework/issues
PackageName: tman
PackageUrl: https://github.com/TEN-framework/ten-framework
License: Apache-2.0
LicenseUrl: https://github.com/TEN-framework/ten-framework/blob/main/LICENSE
ShortDescription: TEN Framework Package Manager
Description: |-
  tman is the official package manager for the TEN Framework.
  It helps developers manage extensions, protocols, and other TEN framework components.
  Features include package installation, dependency resolution, version control, and support for multiple package types.
Tags:
- package-manager
- ten-framework
- extension-manager
- developer-tools
ManifestType: defaultLocale
ManifestVersion: 1.6.0
"@

    $localeManifest | Out-File -FilePath "$manifestDir\TENFramework.tman.locale.en-US.yaml" -Encoding utf8 -NoNewline
}

Write-Success "Generated manifest files:"
Get-ChildItem -Path $manifestDir | ForEach-Object { Write-Host "   - $($_.Name)" }

# Display manifest contents for verification
Write-Info "`nManifest contents:"
Write-Host "==================== Version Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\TENFramework.tman.yaml"
Write-Host "`n==================== Installer Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\TENFramework.tman.installer.yaml"
Write-Host "`n==================== Locale Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\TENFramework.tman.locale.en-US.yaml"
Write-Host ""

# ==============================================================================
# Step 5: Dry Run Check
# ==============================================================================

if ($DryRun) {
    Write-Warning "Dry run mode - skipping PR submission"
    Write-Info "Manifests are ready in: $manifestDir"
    Write-Success "Dry run completed successfully"
    exit 0
}

# ==============================================================================
# Step 6: Fork and Clone winget-pkgs Repository
# ==============================================================================

Write-Info "Preparing winget-pkgs repository..."

# Set GitHub token for gh CLI
$env:GH_TOKEN = $GitHubToken

# Fork the repository (will skip if already forked)
Write-Info "Forking microsoft/winget-pkgs (if not already forked)..."
gh repo fork microsoft/winget-pkgs --clone=false --remote=false 2>&1 | Out-Null

# Get fork owner (current user)
$forkOwner = gh api user --jq '.login'
Write-Info "Fork owner: $forkOwner"

# Clone forked repository
$wingetPkgsDir = "winget-pkgs"
if (Test-Path $wingetPkgsDir) {
    Write-Info "Removing existing winget-pkgs directory..."
    Remove-Item -Path $wingetPkgsDir -Recurse -Force
}

Write-Info "Cloning forked repository..."
git clone "https://x-access-token:$($GitHubToken)@github.com/$forkOwner/winget-pkgs.git" $wingetPkgsDir

Set-Location $wingetPkgsDir

# Configure git
git config user.name "tman-bot"
git config user.email "tman-bot@ten-framework.org"

# Add upstream remote
git remote add upstream https://github.com/microsoft/winget-pkgs.git 2>&1 | Out-Null

# Sync with upstream
Write-Info "Syncing with upstream..."
git fetch upstream
git checkout master
git merge upstream/master
git push origin master

Write-Success "Repository prepared"

# ==============================================================================
# Step 7: Create Branch and Add Manifests
# ==============================================================================

$branchName = "tman-$VersionClean"
Write-Info "Creating branch: $branchName"

git checkout -b $branchName

# Create manifest directory structure
# Winget uses: manifests/<first-letter>/<Publisher>/<Package>/<Version>/
$manifestPath = "manifests/t/TENFramework/tman/$VersionClean"
Write-Info "Creating manifest directory: $manifestPath"
New-Item -ItemType Directory -Force -Path $manifestPath | Out-Null

# Copy manifest files
Write-Info "Copying manifest files..."
Copy-Item -Path "../$manifestDir/*" -Destination $manifestPath -Force

Write-Success "Manifest files copied:"
Get-ChildItem -Path $manifestPath | ForEach-Object { Write-Host "   - $($_.Name)" }

# Stage and commit changes
git add $manifestPath

$commitMsg = "Add TENFramework.tman version $VersionClean"
Write-Info "Committing changes: $commitMsg"
git commit -m $commitMsg

# Push to fork
Write-Info "Pushing to fork..."
git push origin $branchName

Write-Success "Branch pushed successfully"

# ==============================================================================
# Step 8: Create Pull Request
# ==============================================================================

Write-Info "Creating Pull Request to microsoft/winget-pkgs..."

$prBody = @"
## Update TENFramework.tman to version $VersionClean

This PR updates the tman package to version $VersionClean.

### Package Information
- **Package**: TENFramework.tman
- **Version**: $VersionClean
- **Release URL**: https://github.com/$Repository/releases/tag/$Version

### What is tman?
tman is the official package manager for the TEN Framework. It helps developers manage extensions, protocols, and other TEN framework components.

### Changes
- Updated package version to $VersionClean
- Updated installer URL and SHA256 checksum

### Verification
- SHA256 checksum verified: $sha256
- Release asset available at: $assetUrl

---
*This PR was generated using [submit-to-winget.ps1](https://github.com/$Repository/blob/main/tools/winget/submit-to-winget.ps1)*
"@

try {
    gh pr create `
        --repo microsoft/winget-pkgs `
        --title "Update TENFramework.tman to $VersionClean" `
        --body $prBody `
        --head "${forkOwner}:${branchName}" `
        --base master

    Write-Success "Pull Request created successfully!"
    Write-Info "`nNext steps:"
    Write-Info "   1. Microsoft's automated validation will check the PR"
    Write-Info "   2. If validation passes, maintainers will review"
    Write-Info "   3. Once merged, users can install via: winget install TENFramework.tman"
} catch {
    Write-Error "Failed to create Pull Request: $_"
    Write-Info "You may need to create the PR manually at:"
    Write-Info "https://github.com/microsoft/winget-pkgs/compare/master...${forkOwner}:${branchName}"
    exit 1
}

# ==============================================================================
# Cleanup
# ==============================================================================

Set-Location ..
Write-Success "Script completed successfully!"
