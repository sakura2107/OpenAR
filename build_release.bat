@echo off
rem Default CPU build; pass -Profile gpu for CUDA/OCR GPU modules.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
exit /b %ERRORLEVEL%
