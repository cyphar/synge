@echo off
title Synge Installer

:: Synge: A shunting-yard calculation "engine"
:: Copyright (C) 2013, 2016 Aleksa Sarai
::
:: This program is free software: you can redistribute it and/or modify
:: it under the terms of the GNU General Public License as published by
:: the Free Software Foundation, either version 3 of the License, or
:: (at your option) any later version.
::
:: This program is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU General Public License for more details.
::
:: You should have received a copy of the GNU General Public License
:: along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
echo =========== Synge Windows Installer ===========
echo This simple installer will un/install Synge and
echo all of the associated dependancies on a windows
echo system. The files will be downloaded from an
echo online repository, giving the freshest copy of
echo the source tree.
echo ===============================================
echo  i  Install Synge and its dependencies [*] [+]
echo  u  Uninstall Synge and its dependencies [*]
echo  q  Quit installer
echo ===============================================
echo [*] Requires administrative privileges
echo [+] Requires internet access
echo ===============================================

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
copy "synge.dll" "%BIN%\synge.dll"
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

::echo Updating PATH ...
::setx -a PATH %STRIP_PATH%

echo Done
goto end

:help
:: ----
:: Help
:: ----

echo =========== Synge Windows Installer ===========
echo This simple installer will un/install Synge and
echo all of the associated dependancies on a windows
echo system. The files will be downloaded from an
echo online repository, giving the freshest copy of
echo the source tree.
echo ===============================================
echo windows.bat [-i] [-u] [-h]
echo  -i  Install Synge and its dependencies [*] [+]
echo  -u  Uninstall Synge and its dependencies [*]
echo  -q  Quit installer
echo ===============================================
echo [*] Requires administrative privileges
echo [+] Requires internet access
echo ===============================================
goto end

:end
if "%OPTIONS%"=="1" (
	pause >nul
)
