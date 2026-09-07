$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
$src = Join-Path $here "test_sound_input.cpp"
$out = Join-Path $here "sound_input_tests.exe"
Write-Host "Compiling sound-input host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Running..."
& $out
exit $LASTEXITCODE
