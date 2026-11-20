@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0windows\install_jump_pilot.ps1" %*
pause