@echo off
rem Apply w10m-port patches to the upstream submodule checkouts.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0apply-submodule-patches.ps1" %*
exit /b %ERRORLEVEL%
