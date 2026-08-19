# Fetch or update sibling Colyseus plugin for fast-game-ue.
# Usage:
#   .\Scripts\fetch-colyseus.ps1           # install / sync to Scripts\colyseus.lock.json ref
#   .\Scripts\fetch-colyseus.ps1 -Update   # fetch latest from remote default branch tip
#   .\Scripts\fetch-colyseus.ps1 -Ref abc1234
param(
    [switch]$Update,
    [string]$Ref = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$LockPath = Join-Path $PSScriptRoot "colyseus.lock.json"
$Lock = Get-Content -Raw -Encoding UTF8 $LockPath | ConvertFrom-Json

$Repo = $Lock.repo
$TargetRef = if ($Ref) { $Ref } elseif ($Update) { "" } else { $Lock.ref }
$Dest = Join-Path $Root $Lock.dest
$VersionFile = Join-Path $Dest ".colyseus-version"

function Write-VersionFile([string]$Path, [string]$Sha, [string]$RemoteRef) {
    @{
        repo = $Repo
        ref = $RemoteRef
        commit = $Sha
        fetchedAt = (Get-Date).ToUniversalTime().ToString("o")
    } | ConvertTo-Json | Set-Content -Encoding UTF8 $Path
}

function Clone-Colyseus {
    param([string]$CheckoutRef)
    if (Test-Path $Dest) {
        Remove-Item -Recurse -Force $Dest
    }
    $parent = Split-Path -Parent $Dest
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Write-Host "Cloning $Repo -> $Dest"
    if ($CheckoutRef) {
        git clone --recurse-submodules --depth 1 --branch $CheckoutRef $Repo $Dest
    } else {
        git clone --recurse-submodules $Repo $Dest
    }
    if ($Lock.submodules) {
        Push-Location $Dest
        git submodule update --init --recursive
        Pop-Location
    }
    $sha = (git -C $Dest rev-parse HEAD).Trim()
    $resolvedRef = if ($CheckoutRef) { $CheckoutRef } else { "HEAD" }
    Write-VersionFile $VersionFile $sha $resolvedRef
    Write-Host "OK colyseus-unreal @ $sha"
}

function Sync-Colyseus {
    if (-not (Test-Path (Join-Path $Dest ".git"))) {
        Clone-Colyseus -CheckoutRef $TargetRef
        return
    }
    Write-Host "Updating existing clone at $Dest"
    Push-Location $Dest
    git fetch origin
    if ($TargetRef) {
        git checkout $TargetRef
        git pull --ff-only origin $TargetRef 2>$null
        if ($LASTEXITCODE -ne 0) { git pull --ff-only }
    } elseif ($Update) {
        git checkout $Lock.ref
        git pull --ff-only origin $Lock.ref
    } else {
        git pull --ff-only
    }
    if ($Lock.submodules) {
        git submodule update --init --recursive
    }
    Pop-Location
    $sha = (git -C $Dest rev-parse HEAD).Trim()
    $resolvedRef = if ($TargetRef) { $TargetRef } else { "origin/HEAD" }
    Write-VersionFile $VersionFile $sha $resolvedRef
    Write-Host "OK colyseus-unreal @ $sha"
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "git is required"
}

Sync-Colyseus
Write-Host ""
Write-Host "Next: open FastGameUE.uproject and enable plugin 'Colyseus' (colyseus-unreal) if not already enabled."
