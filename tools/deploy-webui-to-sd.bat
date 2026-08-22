@echo off
REM Deploy repo WebUI to the P4 SD card (D:\showduino\webui\)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy-webui-to-sd.ps1" %*
