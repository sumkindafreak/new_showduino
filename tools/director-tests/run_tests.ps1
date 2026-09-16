$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
function Run-One($name, $srcName, $exeName) {
  $src = Join-Path $here $srcName
  $out = Join-Path $here $exeName
  Write-Host "Compiling $name..."
  & g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  Write-Host "Running $name..."
  & $out
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Run-One "Director lamp sheet" "test_lamp_sheet.cpp" "lamp_sheet_tests.exe"
Run-One "Director emergency sheet" "test_emergency_sheet.cpp" "emergency_sheet_tests.exe"
$calInc = Join-Path $here "..\..\firmware\director-esp32-8048s050\ShowduinoDirector8048S050"
Write-Host "Compiling Director touch calibration..."
$calSrc = Join-Path $here "test_touch_calibration.cpp"
$calOut = Join-Path $here "touch_calibration_tests.exe"
& g++ -std=c++17 -Wall -Wextra "-I$calInc" -o $calOut $calSrc
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Running Director touch calibration..."
& $calOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
exit 0
