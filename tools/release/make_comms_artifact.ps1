$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$fqbn = "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB"
$sketch = Join-Path $root "firmware\s3-comms-controller\ShowduinoS3CommsController"
$outDir = Join-Path $root "releases\artifacts"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "Compiling Comms S3 ($fqbn)..."
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
Write-Host "SIZE $size"
Write-Host "SHA256 $hash"

$manifestPath = Join-Path $root "releases\showduino-1.0.0-rc.1.manifest.json"
$json = Get-Content $manifestPath -Raw | ConvertFrom-Json
foreach ($c in $json.components) {
  if ($c.role -eq "comms") {
    $c.firmware = "0.5.0"
    $c.otaCapable = $true
    $c | Add-Member -NotePropertyName hardwareId -NotePropertyValue "SHOWDUINO-S3-COMMS-V1" -Force
    $c | Add-Member -NotePropertyName filename -NotePropertyValue "ShowduinoS3CommsController.ino.bin" -Force
    $c | Add-Member -NotePropertyName size -NotePropertyValue $size -Force
    $c | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash -Force
  }
}
$json.note = "Phase 2A: Comms self-OTA only. System-wide OTA is false. SHA-256 matches the built Comms binary."
($json | ConvertTo-Json -Depth 8) + "`n" | Set-Content -Path $manifestPath -Encoding utf8
Write-Host "Updated $manifestPath"
