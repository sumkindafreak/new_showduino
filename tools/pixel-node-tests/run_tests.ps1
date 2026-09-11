$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
$src = Join-Path $here "test_pixel_node.cpp"
$out = Join-Path $here "pixel_node_tests.exe"
Write-Host "Compiling Pixel Node host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Running..."
& $out
exit $LASTEXITCODE
