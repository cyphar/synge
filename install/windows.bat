@echo off
title Synge Installer

set INSTALL_DIR=C:\Program Files\Synge
set BIN=%INSTALL_DIR%\bin

set ZXA="7za.exe"
set SETX="setx.exe"
set LIBS="win-libs.zip"
set SYNGE="win-synge-release.zip"

set DL="http://dl.dropboxusercontent.com/u/128890973/synge"

if "%1"=="-i" goto install
if "%1"=="-u" goto uninstall
if "%1"=="" goto options
goto help

:options
set OPTIONS=1

cls
echo ========= Synge Windows Installer =========
echo  i  Install Synge and its dependencies
echo  u  Uninstall Synge and its dependencies
echo  q  Quit installer
echo ===========================================

set /p INPUT="option> "

if "%INPUT%"=="i" goto install
if "%INPUT%"=="u" goto uninstall
if "%INPUT%"=="q" goto end
goto options

:install

:: --------
:: Download
:: --------

echo Downloading Tools ...
if not exist %ZXA% (
	wget "%DL%/%ZXA%"
)

if not exist %SETX% (
	wget "%DL%/%SETX%"
)

echo Downloading Libraries ...
if not exist %LIBS% (
	wget "%DL%/%LIBS%"
)

echo Downloading Synge ...
if not exist %SYNGE% (
	wget "%DL%/%SYNGE%"
)

:: -------
:: Extract
:: -------

echo Extracting Libraries ...
%ZXA% x -y %LIBS%

echo Extracting Synge ...
%ZXA% x -y %SYNGE%

:: -------
:: Install
:: -------

echo Installing Libraries ...
mkdir "%INSTALL_DIR%"

echo"%INSTALL_DIR%\bin"

xcopy /s /y /r /i "bin"   "%INSTALL_DIR%\bin"
xcopy /s /y /r /i "etc"   "%INSTALL_DIR%\etc"
xcopy /s /y /r /i "lib"   "%INSTALL_DIR%\lib"
xcopy /s /y /r /i "share" "%INSTALL_DIR%\share"

echo Installing Synge ...
copy "synge-cli.exe" "%BIN%\synge-cli.exe"
copy "synge-cli-colour.exe" "%BIN%\synge-cli-colour.exe"
copy "synge-gtk.exe" "%BIN%\synge-gtk.exe"
copy "synge-eval.exe" "%BIN%\synge-eval.exe"

:: ------
:: Update
:: ------

setx -a PATH "%%C:\Program Files\Synge\bin"

echo Done
goto end

:uninstall
:: ------
:: Remove
:: ------

echo Removing Libraries and Synge ...
rmdir /s /q "%INSTALL_DIR%"

echo Updating PATH ...
::call setx -a PATH %STRIP_PATH%

echo Done
goto end

:help
:: ----
:: Help
:: ----

echo ========= Synge Windows Installer =========
echo windows.bat [-i] [-u] [-h]
echo   -i  Install Synge and its dependencies
echo   -u  Uninstall Synge and its dependencies
echo   -h  Show this help screen
echo ===========================================
goto end

:end
if "%OPTIONS%"=="1" (
	pause >nul
)
