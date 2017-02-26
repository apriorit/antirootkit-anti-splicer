@echo off
set comspec=%windir%\System32\cmd.exe

rem check DDK path
if not "%1" == "" goto SET_BASE_DDK_PATH
echo specify base DDKPath (param 1)
goto ERROR

:SET_BASE_DDK_PATH
SET BASE_DDK_PATH=%1

if not "%2" == "" goto SET_PROJECT_DIR
echo specify project directory (param 2)
goto ERROR

:SET_PROJECT_DIR
SET PROJECT_DIR=%3

if not "%3" == "" goto SET_DDK_CONF_TYPE
echo specify configuration name
goto ERROR

:SET_DDK_CONF_TYPE
SET PROJECT_CONFIGURATION_NAME=%2

if "%2" == "Debug" SET DDK_CONF_TYPE=chk
if "%2" == "Release" SET DDK_CONF_TYPE=free

if "%DDK_CONF_TYPE%" == "" goto ERROR


call %BASE_DDK_PATH%\bin\setenv.bat %BASE_DDK_PATH% %DDK_CONF_TYPE% WXP

SET BUILD_ALLOW_LINKER_WARNINGS=1
cd /d %PROJECT_DIR%
build.exe -I /cE
goto :EOF
:ERROR
echo FATAL ERROR !