$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$root = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
Set-Location $root

$sidecarPath = Join-Path $root "releases\artifacts\comms-artifact.json"
if (-not (Test-Path $sidecarPath)) {
  throw "Missing $sidecarPath — run .\tools\release\make_comms_artifact.ps1 first."
}
$art = Get-Content $sidecarPath -Raw | ConvertFrom-Json
$bin = Join-Path $root $art.bin
$manifest = Join-Path $root $art.manifest
if (-not (Test-Path $bin)) { throw "Missing binary $bin" }
if (-not (Test-Path $manifest)) { throw "Missing manifest $manifest" }

$hash = (Get-FileHash -Algorithm SHA256 $bin).Hash.ToLowerInvariant()
$size = (Get-Item $bin).Length
if ($hash -ne $art.sha256) {
  throw "Sidecar SHA-256 $hash does not match generated $($art.sha256). Rebuild the artifact."
}
if ([int64]$size -ne [int64]$art.size) {
  throw "Sidecar size $size does not match generated $($art.size). Rebuild the artifact."
}

$tag = $art.tag
if (-not $tag) { $tag = "v$($art.product)" }

$notes = @"
Showduino $($art.product) — Comms self-OTA $($art.firmware)

Phase 2A: Communications Controller only. Not system-wide OTA.
hardwareId: SHOWDUINO-S3-COMMS-V1
filename: $($art.filename)
size: $($art.size)
sha256: $($art.sha256)
url: $($art.githubBin)

Boards still on Comms 0.5.0: join Showduino AP, connect venue Wi-Fi on Network, then Diagnostics → SHOWDUINO SOFTWARE → paste the url/firmware/sha256/size into Update Comms.
Do not USB-flash Comms for this update.
"@

$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) { throw "gh CLI not found. Install GitHub CLI and authenticate." }

$exists = $false
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& gh release view $tag --repo sumkindafreak/new_showduino 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) { $exists = $true }
$ErrorActionPreference = $prevEap

$notesFile = Join-Path $env:TEMP "showduino-release-notes-$tag.md"
Set-Content -Path $notesFile -Value $notes -Encoding utf8

if (-not $exists) {
  Write-Host "Creating GitHub Release $tag (prerelease)..."
  & gh release create $tag --repo sumkindafreak/new_showduino --prerelease --title "Showduino $($art.product)" --notes-file $notesFile
  if ($LASTEXITCODE -ne 0) { throw "gh release create failed" }
} else {
  Write-Host "Updating notes on existing GitHub Release $tag..."
  & gh release edit $tag --repo sumkindafreak/new_showduino --notes-file $notesFile
}

Write-Host "Uploading $($art.filename) and manifest..."
& gh release upload $tag $bin $manifest --repo sumkindafreak/new_showduino --clobber
if ($LASTEXITCODE -ne 0) { throw "gh release upload failed" }

Write-Host "PUBLISHED $tag"
Write-Host "VERSION $($art.firmware)"
Write-Host "SIZE $($art.size)"
Write-Host "SHA256 $($art.sha256)"
Write-Host "GITHUB_BIN $($art.githubBin)"
Write-Host "GITHUB_MANIFEST $($art.githubManifest)"
