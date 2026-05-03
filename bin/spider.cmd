@echo off
rem spider.ps1 is in the same directory as this script.
powershell -File "%~dp0spider.ps1" %*
exit /b %ERRORLEVEL%
