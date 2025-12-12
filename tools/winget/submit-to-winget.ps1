<#
.SYNOPSIS
    Submit tman package to Windows Package Manager (winget) repository.

.DESCRIPTION
    This script automates the process of submitting tman to the microsoft/winget-pkgs
    repository, performing the following steps:
    1. Downloads the Windows release asset from GitHub
    2. Calculates or verifies SHA256 checksum
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

.PARAMETER Sha256
    The SHA256 checksum of the release asset.
    If not provided, will calculate it from the downloaded file.

.PARAMETER DryRun
    If specified, will prepare manifests but not submit the PR.
    Only performs steps 1-3.
    Useful for testing and verification.

.EXAMPLE
    .\submit-to-winget.ps1 -Version "0.11.35" -GitHubToken "ghp_xxxxx"

    Submit version 0.11.35 to winget-pkgs.

.EXAMPLE
    .\submit-to-winget.ps1

    Submit the latest version to winget-pkgs.

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
    [string]$Sha256,

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
# Determine Version
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
# Step 1: Download Release Asset
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
# Step 2: Get or Calculate SHA256 Checksum
# ==============================================================================

if ($PSBoundParameters.ContainsKey('Sha256')) {
    # SHA256 parameter was provided, verify it matches the downloaded file
    $sha256 = $Sha256.ToLower()
    Write-Info "SHA256 provided: $sha256"
    Write-Info "Verifying provided SHA256 matches downloaded file..."

    $calculatedSha256 = (Get-FileHash -Path $assetFileName -Algorithm SHA256).Hash.ToLower()

    if ($sha256 -ne $calculatedSha256) {
        Write-Error "SHA256 mismatch!"
        Write-Info "Provided:   $sha256"
        Write-Info "Calculated: $calculatedSha256"
        Write-Info "This indicates the downloaded file does not match the expected hash."
        exit 1
    }

    Write-Success "SHA256 verified ✓"
} else {
    # SHA256 not provided, calculate it from downloaded file
    Write-Info "Calculating SHA256 checksum..."
    $sha256 = (Get-FileHash -Path $assetFileName -Algorithm SHA256).Hash.ToLower()
    Write-Success "SHA256 calculated: $sha256"
}

# ==============================================================================
# Step 3: Generate Manifest Files from Templates
# ==============================================================================

Write-Info "Generating winget manifest files..."

$manifestDir = "winget-manifests"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null

# Check if templates are available
$scriptDir = Split-Path -Parent $PSCommandPath
$requiredTemplates = @(
    "manifest.version.yaml.template",
    "manifest.installer.yaml.template",
    "manifest.locale.en-US.yaml.template"
)

$missingTemplates = @()
foreach ($template in $requiredTemplates) {
    if (-not (Test-Path "$scriptDir\$template")) {
        $missingTemplates += $template
    }
}

if ($missingTemplates.Count -gt 0) {
    Write-Error "Required manifest templates not found in: $scriptDir"
    Write-Info "Missing files:"
    foreach ($template in $missingTemplates) {
        Write-Info "  - $template"
    }
    Write-Info ""
    Write-Info "Please ensure all template files are present in the tools/winget directory."
    exit 1
}

Write-Info "Using templates from: $scriptDir"

# Version Manifest
$versionTemplate = Get-Content "$scriptDir\manifest.version.yaml.template" -Raw
$versionContent = ($versionTemplate -split "`n" | Where-Object {
    $_.Trim() -notmatch '^#' -and $_.Trim() -ne ''
}) -join "`n"
$versionContent = $versionContent -replace '__VERSION__', $VersionClean
$versionContent | Out-File -FilePath "$manifestDir\ten-framework.tman.yaml" -Encoding utf8 -NoNewline

# Installer Manifest
$installerTemplate = Get-Content "$scriptDir\manifest.installer.yaml.template" -Raw
$installerContent = ($installerTemplate -split "`n" | Where-Object {
    $_.Trim() -notmatch '^#' -and $_.Trim() -ne ''
}) -join "`n"
$installerContent = $installerContent -replace '__VERSION__', $Version
$installerContent = $installerContent -replace '__WIN_X64_SHA256__', $sha256
$installerContent | Out-File -FilePath "$manifestDir\ten-framework.tman.installer.yaml" -Encoding utf8 -NoNewline

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
        $localeContent | Out-File -FilePath "$manifestDir\ten-framework.tman.locale.$locale.yaml" -Encoding utf8 -NoNewline
    }
}

Write-Success "Generated manifest files:"
Get-ChildItem -Path $manifestDir | ForEach-Object { Write-Host "   - $($_.Name)" }

# Display manifest contents for verification
Write-Info "`nManifest contents:"
Write-Host "==================== Version Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\ten-framework.tman.yaml"
Write-Host "`n==================== Installer Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\ten-framework.tman.installer.yaml"
Write-Host "`n==================== Locale Manifest ====================" -ForegroundColor Gray
Get-Content "$manifestDir\ten-framework.tman.locale.en-US.yaml"
Write-Host ""

# ==============================================================================
# Dry Run Check
# ==============================================================================

if ($DryRun) {
    Write-Warning "Dry run mode - skipping PR submission"
    Write-Info "Manifests are ready in: $manifestDir"
    Write-Success "Dry run completed successfully"
    exit 0
}

# ==============================================================================
# Step 4: Fork and Clone winget-pkgs Repository
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
git config user.name "ten-framework-bot"
git config user.email "ten-framework-bot@users.noreply.github.com"

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
# Step 5: Create Branch and Add Manifests
# ==============================================================================

$branchName = "tman-$VersionClean"
Write-Info "Preparing branch: $branchName"

# Check if branch already exists on remote
$remoteBranchExists = git ls-remote --heads origin $branchName
if ($remoteBranchExists) {
    Write-Warning "Branch $branchName already exists on remote fork"
    Write-Info "This indicates a previous submission for this version"
    Write-Info "Will update the existing branch (and PR if it exists)"
}

# Check if branch exists locally
$branchExists = git branch --list $branchName
if ($branchExists) {
    Write-Info "Switching to existing local branch: $branchName"
    git checkout $branchName
    # Reset to upstream master to get a clean state
    git reset --hard upstream/master
} else {
    Write-Info "Creating new branch: $branchName"
    git checkout -b $branchName
}

# Create manifest directory structure
# Winget uses: manifests/<first-letter>/<Publisher>/<Package>/<Version>/
$manifestPath = "manifests/t/ten-framework/tman/$VersionClean"
Write-Info "Creating manifest directory: $manifestPath"
New-Item -ItemType Directory -Force -Path $manifestPath | Out-Null

# Copy manifest files
Write-Info "Copying manifest files..."
Copy-Item -Path "../$manifestDir/*" -Destination $manifestPath -Force

Write-Success "Manifest files copied:"
Get-ChildItem -Path $manifestPath | ForEach-Object { Write-Host "   - $($_.Name)" }

# Stage and commit changes
git add $manifestPath

$commitMsg = "Add ten-framework.tman version $VersionClean"
Write-Info "Committing changes: $commitMsg"
git commit -m $commitMsg

# Push to fork
Write-Info "Pushing to fork..."
if ($remoteBranchExists) {
    # Branch exists on remote, force push to update
    Write-Info "Force pushing to update existing branch..."
    git push --force origin $branchName
} else {
    # New branch, normal push
    git push origin $branchName
}

Write-Success "Branch pushed successfully"

# ==============================================================================
# Step 6: Create Pull Request
# ==============================================================================

Write-Info "Preparing Pull Request to microsoft/winget-pkgs..."

# Prepare PR body (used for both new PR and update comment)
$prBody = @"
## Update ten-framework.tman to version $VersionClean

This PR updates the tman package to version $VersionClean.

### Package Information
- **Package**: ten-framework.tman
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

# Check if PR already exists for this branch
Write-Info "Checking if PR already exists..."
# Note: --head parameter only needs branch name, not "owner:branch" format
$existingPR = gh pr list --repo microsoft/winget-pkgs --head "${branchName}" --state open --json number,title 2>&1

if ($LASTEXITCODE -eq 0 -and $existingPR -and $existingPR -ne '[]') {
    $prInfo = $existingPR | ConvertFrom-Json
    if ($prInfo.Count -gt 0) {
        $prNumber = $prInfo[0].number
        Write-Warning "PR already exists for branch ${branchName}"
        Write-Info "Existing PR #${prNumber}: $($prInfo[0].title)"
        Write-Info "URL: https://github.com/microsoft/winget-pkgs/pull/${prNumber}"
        Write-Info ""
        Write-Info "Branch was force-pushed with updates. Adding comment to PR..."

        # Prepare update comment
        $updateComment = @"
## 🔄 Branch Updated

The branch has been updated with the following information:

$prBody

---
**Note**: The PR has been automatically updated by re-running the submission script.
"@

        # Add comment to existing PR
        try {
            gh pr comment $prNumber --repo microsoft/winget-pkgs --body $updateComment
            Write-Success "Comment added to PR #${prNumber}"
        } catch {
            Write-Warning "Failed to add comment to PR: $_"
            Write-Info "You may need to add the update information manually"
        }

        Write-Success "PR #${prNumber} has been updated with new commits"
        Write-Info "`nNext steps:"
        Write-Info "   1. Check the PR for validation results"
        Write-Info "   2. The new commit should trigger re-validation"
        Write-Info "   3. Review the PR at: https://github.com/microsoft/winget-pkgs/pull/${prNumber}"
        exit 0
    }
}

# Create new PR
gh pr create `
    --repo microsoft/winget-pkgs `
    --title "Update ten-framework.tman to $VersionClean" `
    --body $prBody `
    --head "${forkOwner}:${branchName}" `
    --base master

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to create Pull Request"
    Write-Info "You may need to create the PR manually at:"
    Write-Info "https://github.com/microsoft/winget-pkgs/compare/master...${forkOwner}:${branchName}"
    exit 1
}

Write-Success "Pull Request created successfully!"
Write-Info "`nNext steps:"
Write-Info "   1. Microsoft's automated validation will check the PR"
Write-Info "   2. If validation passes, maintainers will review"
Write-Info "   3. Once merged, users can install via: winget install ten-framework.tman"

# ==============================================================================
# Cleanup
# ==============================================================================

Set-Location ..
Write-Success "Script completed successfully!"
