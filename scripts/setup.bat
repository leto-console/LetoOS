@echo off
chcp 65001 > nul

echo [LetoOS] Собираем проект...
setlocal enabledelayedexpansion
cd /d "%~dp0" || exit /b !errorlevel!
call preset_setup.bat LetoOS win-debug || exit /b !errorlevel!
call preset_setup.bat LetoOS stm32f411xe-debug || exit /b !errorlevel!
endlocal
