# Fetch or update sibling Colyseus plugin for fast-game-ue.
# Usage:
#   .\Scripts\fetch-colyseus.ps1           # install / sync to Scripts\colyseus.lock.json ref (SHA or branch)
#   .\Scripts\fetch-colyseus.ps1 -Update   # fetch latest tip of fork main; print SHA to pin in lock
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
$TargetRef = if ($Ref) { $Ref } elseif ($Update) { "main" } else { $Lock.ref }
$Dest = Join-Path $Root $Lock.dest
$VersionFile = Join-Path $Dest ".colyseus-version"

function Test-GitSha([string]$Value) {
    return $Value -match '^[0-9a-fA-F]{7,40}$'
}

function Write-VersionFile([string]$Path, [string]$Sha, [string]$RemoteRef) {
    @{
        repo = $Repo
        ref = $RemoteRef
        commit = $Sha
        upstream = $Lock.upstream
        fetchedAt = (Get-Date).ToUniversalTime().ToString("o")
    } | ConvertTo-Json | Set-Content -Encoding UTF8 $Path
}

function Ensure-Submodules {
    if ($Lock.submodules) {
        Push-Location $Dest
        git submodule update --init --recursive
        Pop-Location
    }
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
    if ((Test-GitSha $CheckoutRef)) {
        git clone --recurse-submodules $Repo $Dest
        Push-Location $Dest
        git checkout $CheckoutRef
        if ($LASTEXITCODE -ne 0) {
            git fetch --depth 1 origin $CheckoutRef
            git checkout $CheckoutRef
        }
        Pop-Location
    } else {
        git clone --recurse-submodules --depth 1 --branch $CheckoutRef $Repo $Dest
    }
    Ensure-Submodules
    $sha = (git -C $Dest rev-parse HEAD).Trim()
    $resolvedRef = if ($CheckoutRef) { $CheckoutRef } else { "HEAD" }
    Write-VersionFile $VersionFile $sha $resolvedRef
    Write-Host "OK colyseus-unreal @ $sha (from $Repo)"
}

function Get-OriginUrl {
    if (-not (Test-Path (Join-Path $Dest ".git"))) { return "" }
    return (git -C $Dest remote get-url origin 2>$null)
}

function Sync-Colyseus {
    $origin = Get-OriginUrl
    $repoNorm = $Repo.TrimEnd('/') -replace '\.git$', ''
    $originNorm = if ($origin) { $origin.TrimEnd('/') -replace '\.git$', '' } else { "" }

    if (-not $origin -or ($originNorm -ne $repoNorm)) {
        if ($origin) {
            Write-Host "Remote mismatch ($origin -> $Repo); re-cloning"
        }
        Clone-Colyseus -CheckoutRef $TargetRef
        return
    }

    Write-Host "Updating existing clone at $Dest"
    Push-Location $Dest
    git fetch origin
    if ((Test-GitSha $TargetRef)) {
        git checkout $TargetRef
        if ($LASTEXITCODE -ne 0) {
            git fetch origin $TargetRef
            git checkout $TargetRef
        }
    } elseif ($Update) {
        git checkout main
        git pull --ff-only origin main
    } else {
        git checkout $TargetRef
        git pull --ff-only origin $TargetRef 2>$null
        if ($LASTEXITCODE -ne 0) { git pull --ff-only }
    }
    Pop-Location
    Ensure-Submodules
    $sha = (git -C $Dest rev-parse HEAD).Trim()
    $resolvedRef = if ($TargetRef) { $TargetRef } else { "origin/HEAD" }
    Write-VersionFile $VersionFile $sha $resolvedRef
    Write-Host "OK colyseus-unreal @ $sha (from $Repo)"
    if ($Update) {
        Write-Host ""
        Write-Host "Pin this SHA in Scripts\colyseus.lock.json `"ref`" after verifying SandboxMultiplayer:"
        Write-Host "  $sha"
        if ($Lock.upstream) {
            Write-Host "Upstream (optional sync into your fork first): $($Lock.upstream)"
        }
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "git is required"
}

Sync-Colyseus
Write-Host ""
Write-Host "Next: open FastGameUE.uproject and enable plugin 'Colyseus' (colyseus-unreal) if not already enabled."
