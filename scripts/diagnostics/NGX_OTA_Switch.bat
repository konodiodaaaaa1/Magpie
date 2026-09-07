@echo off
setlocal EnableExtensions

set "REG_KEY=HKLM\SOFTWARE\NVIDIA Corporation\Global\NGXCore"
set "REG_VALUE=EnableOTA"
set "SELF_PATH=%~f0"

if not "%~1"=="" goto dispatch

call :ensure_admin
if errorlevel 1 exit /b

:menu
cls
echo ============================================================
echo                    NVIDIA NGX OTA Switch
echo ============================================================
call :show_status
echo.
echo   [1] Disable OTA      ^(EnableOTA = 0^)
echo   [2] Enable OTA       ^(EnableOTA = 1^)
echo   [3] Restore default  ^(delete EnableOTA^)
echo   [4] Refresh status
echo   [5] Kill nvngx_update.exe
echo   [6] Exit
echo.
choice /c 123456 /n /m "Select [1-6]: "

if errorlevel 6 goto end
if errorlevel 5 (
    call :clean_processes
    pause
    goto menu
)
if errorlevel 4 goto menu
if errorlevel 3 (
    call :restore_default
    pause
    goto menu
)
if errorlevel 2 (
    call :enable_ota
    pause
    goto menu
)
if errorlevel 1 (
    call :disable_ota
    pause
    goto menu
)

:dispatch
if /i "%~1"=="status" (
    call :show_status
    goto end
)

if /i "%~1"=="off" (
    call :ensure_admin off
    if errorlevel 1 exit /b
    call :disable_ota
    goto end
)

if /i "%~1"=="on" (
    call :ensure_admin on
    if errorlevel 1 exit /b
    call :enable_ota
    goto end
)

if /i "%~1"=="default" (
    call :ensure_admin default
    if errorlevel 1 exit /b
    call :restore_default
    goto end
)

if /i "%~1"=="clean" (
    call :ensure_admin clean
    if errorlevel 1 exit /b
    call :clean_processes
    goto end
)

echo Unknown command: %~1
echo.
echo Commands:
echo   status   Show status
echo   off      Disable OTA
echo   on       Enable OTA
echo   default  Restore default
echo   clean    Kill nvngx_update.exe
exit /b 2

:ensure_admin
fltmc >nul 2>&1
if not errorlevel 1 exit /b 0

echo Administrator access is required. Please approve the UAC prompt.
if "%~1"=="" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%SELF_PATH%' -Verb RunAs"
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%SELF_PATH%' -ArgumentList '%~1' -Verb RunAs"
)

if errorlevel 1 (
    echo UAC request failed or was canceled.
    pause
)
exit /b 1

:show_status
set "OTA_VALUE="
for /f "tokens=3" %%A in ('reg.exe query "%REG_KEY%" /v "%REG_VALUE%" 2^>nul ^| find.exe /i "%REG_VALUE%"') do set "OTA_VALUE=%%A"

echo.
if not defined OTA_VALUE (
    echo Status: ON by default ^(EnableOTA is not set^)
    exit /b 0
)
if /i "%OTA_VALUE%"=="0x0" (
    echo Status: OFF ^(EnableOTA = 0^)
    exit /b 0
)
if /i "%OTA_VALUE%"=="0x1" (
    echo Status: ON ^(EnableOTA = 1^)
    exit /b 0
)
echo Status: Unknown value ^(%OTA_VALUE%^)
exit /b 0

:disable_ota
reg.exe add "%REG_KEY%" /v "%REG_VALUE%" /t REG_DWORD /d 0 /f >nul
if errorlevel 1 (
    echo [ERROR] Registry write failed.
    exit /b 1
)
echo [OK] NGX OTA is disabled.
call :show_status
exit /b 0

:enable_ota
reg.exe add "%REG_KEY%" /v "%REG_VALUE%" /t REG_DWORD /d 1 /f >nul
if errorlevel 1 (
    echo [ERROR] Registry write failed.
    exit /b 1
)
echo [OK] NGX OTA is enabled.
call :show_status
exit /b 0

:restore_default
reg.exe query "%REG_KEY%" /v "%REG_VALUE%" >nul 2>&1
if errorlevel 1 (
    echo [OK] EnableOTA is already unset.
    call :show_status
    exit /b 0
)

reg.exe delete "%REG_KEY%" /v "%REG_VALUE%" /f >nul
if errorlevel 1 (
    echo [ERROR] Registry delete failed.
    exit /b 1
)
echo [OK] NVIDIA default restored.
call :show_status
exit /b 0

:clean_processes
tasklist.exe /fi "imagename eq nvngx_update.exe" 2>nul | find.exe /i "nvngx_update.exe" >nul
if errorlevel 1 (
    echo [OK] No nvngx_update.exe process found.
    exit /b 0
)

taskkill.exe /f /im nvngx_update.exe >nul 2>&1
timeout.exe /t 2 /nobreak >nul
tasklist.exe /fi "imagename eq nvngx_update.exe" 2>nul | find.exe /i "nvngx_update.exe" >nul
if not errorlevel 1 (
    echo [WARN] nvngx_update.exe is still running or was restarted.
    exit /b 1
)
echo [OK] All nvngx_update.exe processes were killed.
exit /b 0

:end
endlocal
exit /b 0
