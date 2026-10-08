@echo off
rem uninstall.cmd - removes what {{SLUG}} copied and wrote (Windows).
rem Started without anything it asks: first whether to remove the copied
rem game files (the data folder's "game", and a "game" left beside the
rem program by an old version), then whether to remove the saves and
rem settings too (the whole data folder).  Only the key Y removes; N or
rem no keyboard to ask on keeps them.  For scripts: --yes answers the
rem first question with yes and the second with no, --yes --all both with
rem yes.  The program's own folder is not touched: it was unpacked by
rem hand and is deleted by hand.
rem No blocks in parentheses below: a folder's name may hold them.
setlocal DisableDelayedExpansion
set "NAME={{NAME}}"
rem the program's folder, taken before the options: shift moves %%0 as well
set "HERE=%~dp0"
set "YES=0"
set "ALL=0"
:args
if "%~1"=="" goto args_done
if /i "%~1"=="--yes" set "YES=1" & shift /1 & goto args
if /i "%~1"=="--all" set "ALL=1" & shift /1 & goto args
goto usage
:args_done
if "%ALL%"=="1" if "%YES%"=="0" goto usage

rem the data folder as the program finds it (the kit's sys_data_dir)
set "DATA=%USERPROFILE%\AppData\Local\%NAME%"
if defined LOCALAPPDATA set "DATA=%LOCALAPPDATA%\%NAME%"
if defined DK_DATA_DIR set "DATA=%DK_DATA_DIR%"
rem never a folder that is not the program's own
if "%DATA%"=="" goto bad_data
if "%DATA:~3%"=="" goto bad_data
if /i "%DATA%"=="%USERPROFILE%" goto bad_data
set "REMOVED=0"

call :game "%DATA%\game"
call :game "%HERE%game"
if exist "%DATA%\" call :rest
if "%REMOVED%"=="0" echo Nothing removed.
echo The program's own folder is yours to delete:
echo   "%HERE%"
if "%YES%"=="0" pause
exit /b 0

:usage
echo usage: uninstall.cmd [--yes [--all]] 1>&2
exit /b 2

:bad_data
echo uninstall.cmd: the data folder is "%DATA%": nothing done 1>&2
exit /b 1

rem the copied game files in the folder %1, if it is there
:game
if not exist "%~1\" goto :eof
call :size "%~1"
call :ask "Remove the copied game files in %~1 (%SIZE%)?" 1
if "%ANSWER%"=="1" goto remove
echo kept "%~1"
goto :eof

rem the whole data folder
:rest
call :size "%DATA%"
call :ask "Also remove the saves and settings in %DATA% (%SIZE%)?" %ALL%
if "%ANSWER%"=="1" call :remove "%DATA%"
if not "%ANSWER%"=="1" echo kept "%DATA%"
goto :eof

:remove
rmdir /s /q "%~1"
if exist "%~1\" echo could not remove "%~1"
if not exist "%~1\" echo removed "%~1"
if not exist "%~1\" set "REMOVED=1"
goto :eof

rem SIZE: the megabytes in the folder %1 (PowerShell counts them)
:size
set "SIZE=?"
set "DK_UNINSTALL_DIR=%~1"
set "DK_UNINSTALL_TMP=%TEMP%\dk-uninstall-%RANDOM%.tmp"
powershell -NoProfile -Command "$s = (Get-ChildItem -LiteralPath $env:DK_UNINSTALL_DIR -Recurse -Force -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum; '{0:N0} MB' -f ($s / 1MB)" > "%DK_UNINSTALL_TMP%" 2>nul
if exist "%DK_UNINSTALL_TMP%" set /p "SIZE=" < "%DK_UNINSTALL_TMP%"
if exist "%DK_UNINSTALL_TMP%" del "%DK_UNINSTALL_TMP%"
goto :eof

rem ANSWER: 1 for yes to the question %1; with --yes it is %2
:ask
set "ANSWER=0"
if "%YES%"=="1" set "ANSWER=%~2" & goto :eof
choice /c NY /n /m "%~1 [y/N] " 2>nul
if errorlevel 255 echo "%~1" [y/N] - no keyboard: not asked, kept
if errorlevel 255 goto :eof
if errorlevel 2 set "ANSWER=1"
goto :eof
