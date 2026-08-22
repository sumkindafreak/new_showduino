# Deploy Showduino Studio WebUI to the P4 SD card (Windows drive D: by default).
# Source of truth remains: web/showduino-studio/
# Runtime copy: <Drive>:\showduino\webui\  ->  ESP32 path /showduino/webui/
#
# Does NOT format the card or delete unrelated Showduino files.

param(
  [string]$Drive = "D:\",
  [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
  $RepoRoot = Split-Path $PSScriptRoot -Parent
}

$src = Join-Path $RepoRoot "web\showduino-studio"
$destRoot = Join-Path $Drive "showduino\webui"

if (-not (Test-Path $src)) {
  throw "WebUI source not found: $src"
}
if (-not (Test-Path $Drive)) {
  throw "SD drive not found: $Drive"
}

New-Item -ItemType Directory -Force -Path $destRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Drive "showduino\audio") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Drive "showduino\shows") | Out-Null

Write-Host "Copying WebUI"
Write-Host "  $src"
Write-Host "  -> $destRoot"

Get-ChildItem -Path $src -Recurse -File | ForEach-Object {
  $rel = $_.FullName.Substring($src.Length).TrimStart('\')
  if ($rel -ieq "README.md") { return }
  $out = Join-Path $destRoot $rel
  $outDir = Split-Path $out -Parent
  if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  }
  Copy-Item -LiteralPath $_.FullName -Destination $out -Force
}

$index = Join-Path $destRoot "index.html"
if (-not (Test-Path $index)) {
  throw "Deploy failed: index.html missing at $index"
}

Write-Host "WebUI deployed."
Write-Host "  Windows: $index"
Write-Host "  ESP32:   /showduino/webui/index.html"
Write-Host "  URL:     http://192.168.4.1/  (SUE AP -> P4 SD origin)"
