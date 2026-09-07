$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$here\embed_webui.py"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
