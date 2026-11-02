$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
$src = Join-Path $here "test_audio_node.cpp"
$out = Join-Path $here "audio_node_tests.exe"
Write-Host "Compiling Audio Node host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Running..."
& $out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$volumeOut = Join-Path $here "audio_volume_tests.exe"
& g++ -std=c++17 -Wall -Wextra -o $volumeOut (Join-Path $here "test_audio_volume.cpp")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $volumeOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& node (Join-Path $here "test_codec_playback.cjs")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& node (Join-Path $here "test_mp3_adapter.cjs")
exit $LASTEXITCODE
