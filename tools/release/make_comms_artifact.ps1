$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$root = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
Set-Location $root

$fqbn = "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB"
$sketch = Join-Path $root "firmware\s3-comms-controller\ShowduinoS3CommsController"
$boardConfig = Join-Path $sketch "BoardConfig.h"
$boardText = Get-Content $boardConfig -Raw
if ($boardText -notmatch '(?m)^\s*#define\s+SHOWDUINO_COMMS_FIRMWARE_VERSION\s+"([^"]+)"') {
  throw "SHOWDUINO_COMMS_FIRMWARE_VERSION not found in $boardConfig"
}
$firmwareVersion = $Matches[1]
if ($firmwareVersion -notmatch '^\d+\.\d+\.\d+') {
  throw "Invalid SHOWDUINO_COMMS_FIRMWARE_VERSION '$firmwareVersion'"
}

$outDir = Join-Path $root "releases\artifacts"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "Compiling Comms S3 $firmwareVersion ($fqbn)..."
& arduino-cli compile --fqbn $fqbn --export-binaries $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$bin = Get-ChildItem -Recurse -Filter "ShowduinoS3CommsController.ino.bin" -Path $sketch |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (-not $bin) { throw "Comms binary not found" }

$dest = Join-Path $outDir "ShowduinoS3CommsController.ino.bin"
Copy-Item $bin.FullName $dest -Force
$hash = (Get-FileHash -Algorithm SHA256 $dest).Hash.ToLowerInvariant()
$size = (Get-Item $dest).Length
Write-Host "BIN  $($bin.FullName)"
Write-Host "VERSION $firmwareVersion"
Write-Host "SIZE $size"
Write-Host "SHA256 $hash"

$versionHeader = Join-Path $root "protocol\showduino_version.h"
$versionText = Get-Content $versionHeader -Raw
$platformVersion = "1.0.0-rc.1"
if ($versionText -match '(?m)^\s*#define\s+SHOWDUINO_PLATFORM_VERSION\s+"([^"]+)"') {
  $platformVersion = $Matches[1]
}
$releaseTag = "v$platformVersion"
$githubBin = "https://github.com/sumkindafreak/new_showduino/releases/download/$releaseTag/ShowduinoS3CommsController.ino.bin"
$githubMan = "https://github.com/sumkindafreak/new_showduino/releases/download/$releaseTag/showduino-$platformVersion.manifest.json"
Write-Host "TAG $releaseTag"
Write-Host "GITHUB_BIN $githubBin"
Write-Host "GITHUB_MANIFEST $githubMan"

$sidecarPath = Join-Path $outDir "comms-artifact.json"
$sidecar = [ordered]@{
  role = "comms"
  firmware = $firmwareVersion
  hardwareId = "SHOWDUINO-S3-COMMS-V1"
  filename = "ShowduinoS3CommsController.ino.bin"
  size = $size
  sha256 = $hash
  product = $platformVersion
  tag = $releaseTag
  bin = "releases/artifacts/ShowduinoS3CommsController.ino.bin"
  manifest = "releases/showduino-$platformVersion.manifest.json"
  githubBin = $githubBin
  githubManifest = $githubMan
}
($sidecar | ConvertTo-Json) + "`n" | Set-Content -Path $sidecarPath -Encoding ascii
Write-Host "SIDECAR $sidecarPath"

$manifestPath = Join-Path $root "releases\showduino-1.0.0-rc.1.manifest.json"
$json = Get-Content $manifestPath -Raw | ConvertFrom-Json
foreach ($c in $json.components) {
  if ($c.role -eq "comms") {
    $c.firmware = $firmwareVersion
    $c.otaCapable = $true
    $c | Add-Member -NotePropertyName hardwareId -NotePropertyValue "SHOWDUINO-S3-COMMS-V1" -Force
    $c | Add-Member -NotePropertyName filename -NotePropertyValue "ShowduinoS3CommsController.ino.bin" -Force
    $c | Add-Member -NotePropertyName size -NotePropertyValue $size -Force
    $c | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash -Force
  }
}
$json.note = "Phase 2A: Comms self-OTA only. System-wide OTA is false. SHA-256 matches the built Comms $firmwareVersion binary."
($json | ConvertTo-Json -Depth 8) + "`n" | Set-Content -Path $manifestPath -Encoding utf8
Write-Host "Updated $manifestPath"
